/* dnssec.c is Copyright (c) 2012 Giovanni Bajo <rasky@develer.com>
           and Copyright (c) 2012-2020 Simon Kelley

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 dated June, 1991, or
   (at your option) version 3 dated 29 June, 2007.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "dnsmasq.h"

#ifdef HAVE_DNSSEC

#define SERIAL_UNDEF  -100
#define SERIAL_EQ        0
#define SERIAL_LT       -1
#define SERIAL_GT        1

/* Convert from presentation format to wire format, in place.
   Also map UC -> LC.
   Note that using extract_name to get presentation format
   then calling to_wire() removes compression and maps case,
   thus generating names in canonical form.
   Calling to_wire followed by from_wire is almost an identity,
   except that the UC remains mapped to LC. 

   Note that both /000 and '.' are allowed within labels. These get
   represented in presentation format using NAME_ESCAPE as an escape
   character. In theory, if all the characters in a name were /000 or
   '.' or NAME_ESCAPE then all would have to be escaped, so the 
   presentation format would be twice as long as the spec (1024). 
   The buffers are all declared as 2049 (allowing for the trailing zero) 
   for this reason.
*/
static int to_wire(char *name)
{
  unsigned char *l, *p, *q, term;
  int len;

  for (l = (unsigned char*)name; *l != 0; l = p)
    {
      for (p = l; *p != '.' && *p != 0; p++)
	if (*p >= 'A' && *p <= 'Z')
	  *p = *p - 'A' + 'a';
	else if (*p == NAME_ESCAPE)
	  {
	    for (q = p; *q; q++)
	      *q = *(q+1);
	    (*p)--;
	  }
      term = *p;
      
      if ((len = p - l) != 0)
	memmove(l+1, l, len);
      *l = len;
      
      p++;
      
      if (term == 0)
	*p = 0;
    }
  
  return l + 1 - (unsigned char *)name;
}

/* Note: no compression  allowed in input. */
static void from_wire(char *name)
{
  unsigned char *l, *p, *last;
  int len;
  
  for (last = (unsigned char *)name; *last != 0; last += *last+1);
  
  for (l = (unsigned char *)name; *l != 0; l += len+1)
    {
      len = *l;
      memmove(l, l+1, len);
      for (p = l; p < l + len; p++)
	if (*p == '.' || *p == 0 || *p == NAME_ESCAPE)
	  {
	    memmove(p+1, p, 1 + last - p);
	    len++;
	    *p++ = NAME_ESCAPE; 
	    (*p)++;
	  }
	
      l[len] = '.';
    }

  if ((char *)l != name)
    *(l-1) = 0;
}

/* Input in presentation format */
static int count_labels(char *name)
{
  int i;
  char *p;
  
  if (*name == 0)
    return 0;

  for (p = name, i = 0; *p; p++)
    if (*p == '.')
      i++;

  /* Don't count empty first label. */
  return *name == '.' ? i : i+1;
}

/* Implement RFC1982 wrapped compare for 32-bit numbers */
static int serial_compare_32(u32 s1, u32 s2)
{
  if (s1 == s2)
    return SERIAL_EQ;

  if ((s1 < s2 && (s2 - s1) < (1UL<<31)) ||
      (s1 > s2 && (s1 - s2) > (1UL<<31)))
    return SERIAL_LT;
  if ((s1 < s2 && (s2 - s1) > (1UL<<31)) ||
      (s1 > s2 && (s1 - s2) < (1UL<<31)))
    return SERIAL_GT;
  return SERIAL_UNDEF;
}

/* Called at startup. If the timestamp file is configured and exists, put its mtime on
   timestamp_time. If it doesn't exist, create it, and set the mtime to 1-1-2015.
   return -1 -> Cannot create file.
           0 -> not using timestamp, or timestamp exists and is in past.
           1 -> timestamp exists and is in future.
*/

static time_t timestamp_time;

int setup_timestamp(void)
{
  struct stat statbuf;
  
  daemon->back_to_the_future = 0;
  
  if (!daemon->timestamp_file)
    return 0;
  
  if (stat(daemon->timestamp_file, &statbuf) != -1)
    {
      timestamp_time = statbuf.st_mtime;
    check_and_exit:
      if (difftime(timestamp_time, time(0)) <=  0)
	{
	  /* time already OK, update timestamp, and do key checking from the start. */
	  if (utimes(daemon->timestamp_file, NULL) == -1)
	    my_syslog(LOG_ERR, _("failed to update mtime on %s: %s"), daemon->timestamp_file, strerror(errno));
	  daemon->back_to_the_future = 1;
	  return 0;
	}
      return 1;
    }
  
  if (errno == ENOENT)
    {
      /* NB. for explanation of O_EXCL flag, see comment on pidfile in dnsmasq.c */ 
      int fd = open(daemon->timestamp_file, O_WRONLY | O_CREAT | O_NONBLOCK | O_EXCL, 0666);
      if (fd != -1)
	{
	  struct timeval tv[2];

	  close(fd);
	  
	  timestamp_time = 1420070400; /* 1-1-2015 */
	  tv[0].tv_sec = tv[1].tv_sec = timestamp_time;
	  tv[0].tv_usec = tv[1].tv_usec = 0;
	  if (utimes(daemon->timestamp_file, tv) == 0)
	    goto check_and_exit;
	}
    }

  return -1;
}

/* Check whether today/now is between date_start and date_end */
static int is_check_date(unsigned long curtime)
{
  /* Checking timestamps may be temporarily disabled */
    
  /* If the current time if _before_ the timestamp
     on our persistent timestamp file, then assume the
     time if not yet correct, and don't check the
     key timestamps. As soon as the current time is
     later then the timestamp, update the timestamp
     and start checking keys */
  if (daemon->timestamp_file)
    {
      if (daemon->back_to_the_future == 0 && difftime(timestamp_time, curtime) <= 0)
	{
	  if (utimes(daemon->timestamp_file, NULL) != 0)
	    my_syslog(LOG_ERR, _("failed to update mtime on %s: %s"), daemon->timestamp_file, strerror(errno));
	  
	  my_syslog(LOG_INFO, _("system time considered valid, now checking DNSSEC signature timestamps."));
	  daemon->back_to_the_future = 1;
	  daemon->dnssec_no_time_check = 0;
	  queue_event(EVENT_RELOAD); /* purge cache */
	} 

      return daemon->back_to_the_future;
    }
  else
    return !daemon->dnssec_no_time_check;
}

/* Check whether today/now is between date_start and date_end */
static int check_date_range(unsigned long curtime, u32 date_start, u32 date_end)
{
  /* We must explicitly check against wanted values, because of SERIAL_UNDEF */
  return serial_compare_32(curtime, date_start) == SERIAL_GT
    && serial_compare_32(curtime, date_end) == SERIAL_LT;
}

/* Return bytes of canonicalised rdata, when the return value is zero, the remaining 
   data, pointed to by *p, should be used raw. */
static int get_rdata(struct dns_header *header, size_t plen, unsigned char *end, char *buff, int bufflen,
		     unsigned char **p, u16 **desc)
{
  int d = **desc;
  
  /* No more data needs mangling */
  if (d == (u16)-1)
    {
      /* If there's more data than we have space for, just return what fits,
	 we'll get called again for more chunks */
      if (end - *p > bufflen)
	{
	  memcpy(buff, *p, bufflen);
	  *p += bufflen;
	  return bufflen;
	}
      
      return 0;
    }
 
  (*desc)++;
  
  if (d == 0 && extract_name(header, plen, p, buff, 1, 0))
    /* domain-name, canonicalise */
    return to_wire(buff);
  else
    { 
      /* plain data preceding a domain-name, don't run off the end of the data */
      if ((end - *p) < d)
	d = end - *p;
      
      if (d != 0)
	{
	  memcpy(buff, *p, d);
	  *p += d;
	}
      
      return d;
    }
}

/* Bubble sort the RRset into the canonical order. 
   Note that the byte-streams from two RRs may get unsynced: consider 
   RRs which have two domain-names at the start and then other data.
   The domain-names may have different lengths in each RR, but sort equal

   ------------
   |abcde|fghi|
   ------------
   |abcd|efghi|
   ------------

   leaving the following bytes as deciding the order. Hence the nasty left1 and left2 variables.
*/

static int sort_rrset(struct dns_header *header, size_t plen, u16 *rr_desc, int rrsetidx, 
		      unsigned char **rrset, char *buff1, char *buff2)
{
  int swap, quit, i, j;
  
  do
    {
      for (swap = 0, i = 0; i < rrsetidx-1; i++)
	{
	  int rdlen1, rdlen2, left1, left2, len1, len2, len, rc;
	  u16 *dp1, *dp2;
	  unsigned char *end1, *end2;
	  /* Note that these have been determined to be OK previously,
	     so we don't need to check for NULL return here. */
	  unsigned char *p1 = skip_name(rrset[i], header, plen, 10);
	  unsigned char *p2 = skip_name(rrset[i+1], header, plen, 10);
	  
	  p1 += 8; /* skip class, type, ttl */
	  GETSHORT(rdlen1, p1);
	  end1 = p1 + rdlen1;
	  
	  p2 += 8; /* skip class, type, ttl */
	  GETSHORT(rdlen2, p2);
	  end2 = p2 + rdlen2; 
	  
	  dp1 = dp2 = rr_desc;
	  
	  for (quit = 0, left1 = 0, left2 = 0, len1 = 0, len2 = 0; !quit;)
	    {
	      if (left1 != 0)
		memmove(buff1, buff1 + len1 - left1, left1);
	      
	      if ((len1 = get_rdata(header, plen, end1, buff1 + left1, (MAXDNAME * 2) - left1, &p1, &dp1)) == 0)
		{
		  quit = 1;
		  len1 = end1 - p1;
		  memcpy(buff1 + left1, p1, len1);
		}
	      len1 += left1;
	      
	      if (left2 != 0)
		memmove(buff2, buff2 + len2 - left2, left2);
	      
	      if ((len2 = get_rdata(header, plen, end2, buff2 + left2, (MAXDNAME *2) - left2, &p2, &dp2)) == 0)
		{
		  quit = 1;
		  len2 = end2 - p2;
		  memcpy(buff2 + left2, p2, len2);
		}
	      len2 += left2;
	       
	      if (len1 > len2)
		left1 = len1 - len2, left2 = 0, len = len2;
	      else
		left2 = len2 - len1, left1 = 0, len = len1;
	      
	      rc = (len == 0) ? 0 : memcmp(buff1, buff2, len);
	      
	      if (rc > 0 || (rc == 0 && quit && len1 > len2))
		{
		  unsigned char *tmp = rrset[i+1];
		  rrset[i+1] = rrset[i];
		  rrset[i] = tmp;
		  swap = quit = 1;
		}
	      else if (rc == 0 && quit && len1 == len2)
		{
		  /* Two RRs are equal, remove one copy. RFC 4034, para 6.3 */
		  for (j = i+1; j < rrsetidx-1; j++)
		    rrset[j] = rrset[j+1];
		  rrsetidx--;
		  i--;
		}
	      else if (rc < 0)
		quit = 1;
	    }
	}
    } while (swap);

  return rrsetidx;
}

static unsigned char **rrset = NULL, **sigs = NULL;

/* Get pointers to RRset members and signature(s) for same.
   Check signatures, and return keyname associated in keyname. */
static int explore_rrset(struct dns_header *header, size_t plen, int class, int type, 
			 char *name, char *keyname, int *sigcnt, int *rrcnt)
{
  static int rrset_sz = 0, sig_sz = 0; 
  unsigned char *p;
  int rrsetidx, sigidx, j, rdlen, res;
  int gotkey = 0;

  if (!(p = skip_questions(header, plen)))
    return 0;

   /* look for RRSIGs for this RRset and get pointers to each RR in the set. */
  for (rrsetidx = 0, sigidx = 0, j = ntohs(header->ancount) + ntohs(header->nscount); 
       j != 0; j--) 
    {
      unsigned char *pstart, *pdata;
      int stype, sclass, type_covered;

      pstart = p;
      
      if (!(res = extract_name(header, plen, &p, name, 0, 10)))
	return 0; /* bad packet */
      
      GETSHORT(stype, p);
      GETSHORT(sclass, p);
           
      pdata = p;

      p += 4; /* TTL */
      GETSHORT(rdlen, p);
      
      if (!CHECK_LEN(header, p, plen, rdlen))
	return 0; 
      
      if (res == 1 && sclass == class)
	{
	  if (stype == type)
	    {
	      if (!expand_workspace(&rrset, &rrset_sz, rrsetidx))
		return 0; 
	      
	      rrset[rrsetidx++] = pstart;
	    }
	  
	  if (stype == T_RRSIG)
	    {
	      if (rdlen < 18)
		return 0; /* bad packet */ 
	      
	      GETSHORT(type_covered, p);
	      p += 16; /* algo, labels, orig_ttl, sig_expiration, sig_inception, key_tag */
	      
	      if (gotkey)
		{
		  /* If there's more than one SIG, ensure they all have same keyname */
		  if (extract_name(header, plen, &p, keyname, 0, 0) != 1)
		    return 0;
		}
	      else
		{
		  gotkey = 1;
		  
		  if (!extract_name(header, plen, &p, keyname, 1, 0))
		    return 0;
		  
		  /* RFC 4035 5.3.1 says that the Signer's Name field MUST equal
		     the name of the zone containing the RRset. We can't tell that
		     for certain, but we can check that  the RRset name is equal to
		     or encloses the signers name, which should be enough to stop 
		     an attacker using signatures made with the key of an unrelated 
		     zone he controls. Note that the root key is always allowed. */
		  if (*keyname != 0)
		    {
		      char *name_start;
		      for (name_start = name; !hostname_isequal(name_start, keyname); )
			if ((name_start = strchr(name_start, '.')))
			  name_start++; /* chop a label off and try again */
			else
			  return 0;
		    }
		}
		  
	      
	      if (type_covered == type)
		{
		  if (!expand_workspace(&sigs, &sig_sz, sigidx))
		    return 0; 
		  
		  sigs[sigidx++] = pdata;
		} 
	      
	      p = pdata + 6; /* restore for ADD_RDLEN */
	    }
	}
      
      if (!ADD_RDLEN(header, p, plen, rdlen))
	return 0;
    }
  
  *sigcnt = sigidx;
  *rrcnt = rrsetidx;

  return 1;
}

/* Validate a single RRset (class, type, name) in the supplied DNS reply 
   Return code:
   STAT_SECURE   if it validates.
   STAT_SECURE_WILDCARD if it validates and is the result of wildcard expansion.
   (In this case *wildcard_out points to the "body" of the wildcard within name.) 
   STAT_BOGUS    signature is wrong, bad packet.
   STAT_NEED_KEY need DNSKEY to complete validation (name is returned in keyname)
   STAT_NEED_DS  need DS to complete validation (name is returned in keyname)

   If key is non-NULL, use that key, which has the algo and tag given in the params of those names,
   otherwise find the key in the cache.

   Name is unchanged on exit. keyname is used as workspace and trashed.

   Call explore_rrset first to find and count RRs and sigs.

   ttl_out is the floor on TTL, based on TTL and orig_ttl and expiration of sig used to validate.
*/
static int validate_rrset(time_t now, struct dns_header *header, size_t plen, int class, int type, int sigidx, int rrsetidx, 
			  char *name, char *keyname, char **wildcard_out, struct blockdata *key, int keylen,
			  int algo_in, int keytag_in, unsigned long *ttl_out)
{
  unsigned char *p;
  int rdlen, j, name_labels, algo, labels, key_tag;
  struct crec *crecp = NULL;
  u16 *rr_desc = rrfilter_desc(type);
  u32 sig_expiration, sig_inception;

  unsigned long curtime = time(0);
  int time_check = is_check_date(curtime);
  
  if (wildcard_out)
    *wildcard_out = NULL;
  
  name_labels = count_labels(name); /* For 4035 5.3.2 check */

  /* Sort RRset records into canonical order. 
     Note that at this point keyname and daemon->workspacename buffs are
     unused, and used as workspace by the sort. */
  rrsetidx = sort_rrset(header, plen, rr_desc, rrsetidx, rrset, daemon->workspacename, keyname);
         
  /* Now try all the sigs to try and find one which validates */
  for (j = 0; j <sigidx; j++)
    {
      unsigned char *psav, *sig, *digest;
      int i, wire_len, sig_len;
      const void *hash;
      void *ctx;
      char *name_start;
      u32 nsigttl, ttl, orig_ttl;
      
      p = sigs[j];
      GETLONG(ttl, p);
      GETSHORT(rdlen, p); /* rdlen >= 18 checked previously */
      psav = p;
      
      p += 2; /* type_covered - already checked */
      algo = *p++;
      labels = *p++;
      GETLONG(orig_ttl, p);
      GETLONG(sig_expiration, p);
      GETLONG(sig_inception, p);
      GETSHORT(key_tag, p);
      
      if (!extract_name(header, plen, &p, keyname, 1, 0))
	return STAT_BOGUS;

      if ((time_check && !check_date_range(curtime, sig_inception, sig_expiration)) ||
	  labels > name_labels ||
	  !(hash = hash_find(algo_digest_name(algo))) ||
	  !hash_init(hash, &ctx, &digest))
	continue;

      /* OK, we have the signature record, see if the relevant DNSKEY is in the cache. */
      if (!key && !(crecp = cache_find_by_name(NULL, keyname, now, F_DNSKEY)))
	return STAT_NEED_KEY;

       if (ttl_out)
	 {
	   /* 4035 5.3.3 rules on TTLs */
	   if (orig_ttl < ttl)
	     ttl = orig_ttl;
	   
	   if (time_check && difftime(sig_expiration, curtime) < ttl)
	     ttl = difftime(sig_expiration, curtime);

	   *ttl_out = ttl;
	 }
       
      sig = p;
      sig_len = rdlen - (p - psav);
              
      nsigttl = htonl(orig_ttl);
      
      hash_update(hash, ctx, 18, psav);
      wire_len = to_wire(keyname);
      hash_update(hash, ctx, (unsigned int)wire_len, (unsigned char*)keyname);
      from_wire(keyname);
      
      for (i = 0; i < rrsetidx; ++i)
	{
	  int seg;
	  unsigned char *end, *cp;
	  u16 len, *dp;
	  
	  p = rrset[i];
	 	  
	  if (!extract_name(header, plen, &p, name, 1, 10)) 
	    return STAT_BOGUS;

	  name_start = name;
	  
	  /* if more labels than in RRsig name, hash *.<no labels in rrsig labels field>  4035 5.3.2 */
	  if (labels < name_labels)
	    {
	      int k;
	      for (k = name_labels - labels; k != 0; k--)
		{
		  while (*name_start != '.' && *name_start != 0)
		    name_start++;
		  if (k != 1 && *name_start == '.')
		    name_start++;
		}
	      
	      if (wildcard_out)
		*wildcard_out = name_start+1;

	      name_start--;
	      *name_start = '*';
	    }
	  
	  wire_len = to_wire(name_start);
	  hash_update(hash, ctx, (unsigned int)wire_len, (unsigned char *)name_start);
	  hash_update(hash, ctx, 4, p); /* class and type */
	  hash_update(hash, ctx, 4, (unsigned char *)&nsigttl);
	  
	  p += 8; /* skip class, type, ttl */
	  GETSHORT(rdlen, p);
	  if (!CHECK_LEN(header, p, plen, rdlen))
	    return STAT_BOGUS; 
	  
	  end = p + rdlen;
	  
	  /* canonicalise rdata and calculate length of same, use name buffer as workspace.
	     Note that name buffer is twice MAXDNAME long in DNSSEC mode. */
	  cp = p;
	  dp = rr_desc;
	  for (len = 0; (seg = get_rdata(header, plen, end, name, MAXDNAME * 2, &cp, &dp)) != 0; len += seg);
	  len += end - cp;
	  len = htons(len);
	  hash_update(hash, ctx, 2, (unsigned char *)&len); 
	  
	  /* Now canonicalise again and digest. */
	  cp = p;
	  dp = rr_desc;
	  while ((seg = get_rdata(header, plen, end, name, MAXDNAME * 2, &cp, &dp)))
	    hash_update(hash, ctx, seg, (unsigned char *)name);
	  if (cp != end)
	    hash_update(hash, ctx, end - cp, cp);
	}
     
      hash_digest(hash, ctx, hash_length(hash), digest);
      
      /* namebuff used for workspace above, restore to leave unchanged on exit */
      p = (unsigned char*)(rrset[0]);
      extract_name(header, plen, &p, name, 1, 0);

      if (key)
	{
	  if (algo_in == algo && keytag_in == key_tag &&
	      verify(key, keylen, sig, sig_len, digest, hash_length(hash), algo))
	    return STAT_SECURE;
	}
      else
	{
	  /* iterate through all possible keys 4035 5.3.1 */
	  for (; crecp; crecp = cache_find_by_name(crecp, keyname, now, F_DNSKEY))
	    if (crecp->addr.key.algo == algo && 
		crecp->addr.key.keytag == key_tag &&
		crecp->uid == (unsigned int)class &&
		verify(crecp->addr.key.keydata, crecp->addr.key.keylen, sig, sig_len, digest, hash_length(hash), algo))
	      return (labels < name_labels) ? STAT_SECURE_WILDCARD : STAT_SECURE;
	}
    }

  return STAT_BOGUS;
}
 

/* The DNS packet is expected to contain the answer to a DNSKEY query.
   Put all DNSKEYs in the answer which are valid into the cache.
   return codes:
         STAT_OK        Done, key(s) in cache.
	 STAT_BOGUS     No DNSKEYs found, which  can be validated with DS,
	                or self-sign for DNSKEY RRset is not valid, bad packet.
	 STAT_NEED_DS   DS records to validate a key not found, name in keyname 
	 STAT_NEED_KEY  DNSKEY records to validate a key not found, name in keyname 
*/
int dnssec_validate_by_ds(time_t now, struct dns_header *header, size_t plen, char *name, char *keyname, int class)
{
  unsigned char *psave, *p = (unsigned char *)(header+1);
  struct crec *crecp, *recp1;
  int rc, j, qtype, qclass, rdlen, flags, algo, valid, keytag;
  unsigned long ttl, sig_ttl;
  struct blockdata *key;
  union all_addr a;

  if (ntohs(header->qdcount) != 1 ||
      RCODE(header) == SERVFAIL || RCODE(header) == REFUSED ||
      !extract_name(header, plen, &p, name, 1, 4))
    return STAT_BOGUS;

  GETSHORT(qtype, p);
  GETSHORT(qclass, p);
  
  if (qtype != T_DNSKEY || qclass != class || ntohs(header->ancount) == 0)
    return STAT_BOGUS;

  /* See if we have cached a DS record which validates this key */
  if (!(crecp = cache_find_by_name(NULL, name, now, F_DS)))
    {
      strcpy(keyname, name);
      return STAT_NEED_DS;
    }
  
  /* NOTE, we need to find ONE DNSKEY which matches the DS */
  for (valid = 0, j = ntohs(header->ancount); j != 0 && !valid; j--) 
    {
      /* Ensure we have type, class  TTL and length */
      if (!(rc = extract_name(header, plen, &p, name, 0, 10)))
	return STAT_BOGUS; /* bad packet */
  
      GETSHORT(qtype, p); 
      GETSHORT(qclass, p);
      GETLONG(ttl, p);
      GETSHORT(rdlen, p);
 
      if (!CHECK_LEN(header, p, plen, rdlen) || rdlen < 4)
	return STAT_BOGUS; /* bad packet */
      
      if (qclass != class || qtype != T_DNSKEY || rc == 2)
	{
	  p += rdlen;
	  continue;
	}
            
      psave = p;
      
      GETSHORT(flags, p);
      if (*p++ != 3)
	return STAT_BOGUS;
      algo = *p++;
      keytag = dnskey_keytag(algo, flags, p, rdlen - 4);
      key = NULL;
      
      /* key must have zone key flag set */
      if (flags & 0x100)
	key = blockdata_alloc((char*)p, rdlen - 4);
      
      p = psave;
      
      if (!ADD_RDLEN(header, p, plen, rdlen))
	{
	  if (key)
	    blockdata_free(key);
	  return STAT_BOGUS; /* bad packet */
	}

      /* No zone key flag or malloc failure */
      if (!key)
	continue;
      
      for (recp1 = crecp; recp1; recp1 = cache_find_by_name(recp1, name, now, F_DS))
	{
	  void *ctx;
	  unsigned char *digest, *ds_digest;
	  const void *hash;
	  int sigcnt, rrcnt;

	  if (recp1->addr.ds.algo == algo && 
	      recp1->addr.ds.keytag == keytag &&
	      recp1->uid == (unsigned int)class &&
	      (hash = hash_find(ds_digest_name(recp1->addr.ds.digest))) &&
	      hash_init(hash, &ctx, &digest))
	    {
	      int wire_len = to_wire(name);
	      
	      /* Note that digest may be different between DSs, so 
		 we can't move this outside the loop. */
	      hash_update(hash, ctx, (unsigned int)wire_len, (unsigned char *)name);
	      hash_update(hash, ctx, (unsigned int)rdlen, psave);
	      hash_digest(hash, ctx, hash_length(hash), digest);
	      
	      from_wire(name);
	      
	      if (!(recp1->flags & F_NEG) &&
		  recp1->addr.ds.keylen == (int)hash_length(hash) &&
		  (ds_digest = blockdata_retrieve(recp1->addr.ds.keydata, recp1->addr.ds.keylen, NULL)) &&
		  memcmp(ds_digest, digest, recp1->addr.ds.keylen) == 0 &&
		  explore_rrset(header, plen, class, T_DNSKEY, name, keyname, &sigcnt, &rrcnt) &&
		  sigcnt != 0 && rrcnt != 0 &&
		  validate_rrset(now, header, plen, class, T_DNSKEY, sigcnt, rrcnt, name, keyname, 
				 NULL, key, rdlen - 4, algo, keytag, &sig_ttl) == STAT_SECURE)
		{
		  valid = 1;
		  break;
		}
	    }
	}
      blockdata_free(key);
    }

  if (valid)
    {
      /* DNSKEY RRset determined to be OK, now cache it. */
      cache_start_insert();
      
      p = skip_questions(header, plen);

      for (j = ntohs(header->ancount); j != 0; j--) 
	{
	  /* Ensure we have type, class  TTL and length */
	  if (!(rc = extract_name(header, plen, &p, name, 0, 10)))
	    return STAT_BOGUS; /* bad packet */
	  
	  GETSHORT(qtype, p); 
	  GETSHORT(qclass, p);
	  GETLONG(ttl, p);
	  GETSHORT(rdlen, p);

	  /* TTL may be limited by sig. */
	  if (sig_ttl < ttl)
	    ttl = sig_ttl;
	    
	  if (!CHECK_LEN(header, p, plen, rdlen))
	    return STAT_BOGUS; /* bad packet */
	  
	  if (qclass == class && rc == 1)
	    {
	      psave = p;
	      
	      if (qtype == T_DNSKEY)
		{
		  if (rdlen < 4)
		    return STAT_BOGUS; /* bad packet */
		  
		  GETSHORT(flags, p);
		  if (*p++ != 3)
		    return STAT_BOGUS;
		  algo = *p++;
		  keytag = dnskey_keytag(algo, flags, p, rdlen - 4);
		  
		  if ((key = blockdata_alloc((char*)p, rdlen - 4)))
		    {
		      a.key.keylen = rdlen - 4;
		      a.key.keydata = key;
		      a.key.algo = algo;
		      a.key.keytag = keytag;
		      a.key.flags = flags;
		      
		      if (!cache_insert(name, &a, class, now, ttl, F_FORWARD | F_DNSKEY | F_DNSSECOK))
			{
			  blockdata_free(key);
			  return STAT_BOGUS;
			}
		      else
			{
			  a.log.keytag = keytag;
			  a.log.algo = algo;
			  if (algo_digest_name(algo))
			    log_query(F_NOEXTRA | F_KEYTAG | F_UPSTREAM, name, &a, "DNSKEY keytag %hu, algo %hu");
			  else
			    log_query(F_NOEXTRA | F_KEYTAG | F_UPSTREAM, name, &a, "DNSKEY keytag %hu, algo %hu (not supported)");
			}
		    }
		}
	      	      
	      p = psave;
	    }

	  if (!ADD_RDLEN(header, p, plen, rdlen))
	    return STAT_BOGUS; /* bad packet */
	}
      
      /* commit cache insert. */
      cache_end_insert();
      return STAT_OK;
    }

  log_query(F_NOEXTRA | F_UPSTREAM, name, NULL, "BOGUS DNSKEY");
  return STAT_BOGUS;
}

/* The DNS packet is expected to contain the answer to a DS query
   Put all DSs in the answer which are valid into the cache.
   Also handles replies which prove that there's no DS at this location, 
   either because the zone is unsigned or this isn't a zone cut. These are
   cached too.
   return codes:
   STAT_OK          At least one valid DS found and in cache.
   STAT_BOGUS       no DS in reply or not signed, fails validation, bad packet.
   STAT_NEED_KEY    DNSKEY records to validate a DS not found, name in keyname
   STAT_NEED_DS     DS record needed.
*/

int dnssec_validate_ds(time_t now, struct dns_header *header, size_t plen, char *name, char *keyname, int class)
{
  unsigned char *p = (unsigned char *)(header+1);
  int qtype, qclass, rc, i, neganswer, nons, neg_ttl = 0;
  int aclass, atype, rdlen;
  unsigned long ttl;
  union all_addr a;

  if (ntohs(header->qdcount) != 1 ||
      !(p = skip_name(p, header, plen, 4)))
    return STAT_BOGUS;
  
  GETSHORT(qtype, p);
  GETSHORT(qclass, p);

  if (qtype != T_DS || qclass != class)
    rc = STAT_BOGUS;
  else
    rc = dnssec_validate_reply(now, header, plen, name, keyname, NULL, 0, &neganswer, &nons, &neg_ttl);
  
  if (rc == STAT_INSECURE)
    {
      my_syslog(LOG_WARNING, _("Insecure DS reply received for %s, check domain configuration and upstream DNS server DNSSEC support"), name);
      rc = STAT_BOGUS;
    }
  
  p = (unsigned char *)(header+1);
  extract_name(header, plen, &p, name, 1, 4);
  p += 4; /* qtype, qclass */
  
  /* If the key needed to validate the DS is on the same domain as the DS, we'll
     loop getting nowhere. Stop that now. This can happen of the DS answer comes
     from the DS's zone, and not the parent zone. */
  if (rc == STAT_BOGUS || (rc == STAT_NEED_KEY && hostname_isequal(name, keyname)))
    {
      log_query(F_NOEXTRA | F_UPSTREAM, name, NULL, "BOGUS DS");
      return STAT_BOGUS;
    }
  
  if (rc != STAT_SECURE)
    return rc;
   
  if (!neganswer)
    {
      cache_start_insert();
      
      for (i = 0; i < ntohs(header->ancount); i++)
	{
	  if (!(rc = extract_name(header, plen, &p, name, 0, 10)))
	    return STAT_BOGUS; /* bad packet */
	  
	  GETSHORT(atype, p);
	  GETSHORT(aclass, p);
	  GETLONG(ttl, p);
	  GETSHORT(rdlen, p);
	  
	  if (!CHECK_LEN(header, p, plen, rdlen))
	    return STAT_BOGUS; /* bad packet */
	  
	  if (aclass == class && atype == T_DS && rc == 1)
	    { 
	      int algo, digest, keytag;
	      unsigned char *psave = p;
	      struct blockdata *key;
	   
	      if (rdlen < 4)
		return STAT_BOGUS; /* bad packet */
	      
	      GETSHORT(keytag, p);
	      algo = *p++;
	      digest = *p++;
	      
	      if ((key = blockdata_alloc((char*)p, rdlen - 4)))
		{
		  a.ds.digest = digest;
		  a.ds.keydata = key;
		  a.ds.algo = algo;
		  a.ds.keytag = keytag;
		  a.ds.keylen = rdlen - 4;

		  if (!cache_insert(name, &a, class, now, ttl, F_FORWARD | F_DS | F_DNSSECOK))
		    {
		      blockdata_free(key);
		      return STAT_BOGUS;
		    }
		  else
		    {
		      a.log.keytag = keytag;
		      a.log.algo = algo;
		      a.log.digest = digest;
		      if (ds_digest_name(digest) && algo_digest_name(algo))
			log_query(F_NOEXTRA | F_KEYTAG | F_UPSTREAM, name, &a, "DS keytag %hu, algo %hu, digest %hu");
		      else
			log_query(F_NOEXTRA | F_KEYTAG | F_UPSTREAM, name, &a, "DS keytag %hu, algo %hu, digest %hu (not supported)");
		    } 
		}
	      
	      p = psave;
	    }
	  if (!ADD_RDLEN(header, p, plen, rdlen))
	    return STAT_BOGUS; /* bad packet */
	}

      cache_end_insert();

    }
  else
    {
      int flags = F_FORWARD | F_DS | F_NEG | F_DNSSECOK;
            
      if (RCODE(header) == NXDOMAIN)
	flags |= F_NXDOMAIN;
      
      /* We only cache validated DS records, DNSSECOK flag hijacked 
	 to store presence/absence of NS. */
      if (nons)
	flags &= ~F_DNSSECOK;
      
      cache_start_insert();
	  
      /* Use TTL from NSEC for negative cache entries */
      if (!cache_insert(name, NULL, class, now, neg_ttl, flags))
	return STAT_BOGUS;
      
      cache_end_insert();  
      
      log_query(F_NOEXTRA | F_UPSTREAM, name, NULL, "no DS");
    }
      
  return STAT_OK;
}


/* 4034 6.1 */
static int hostname_cmp(const char *a, const char *b)
{
  char *sa, *ea, *ca, *sb, *eb, *cb;
  unsigned char ac, bc;
  
  sa = ea = (char *)a + strlen(a);
  sb = eb = (char *)b + strlen(b);
 
  while (1)
    {
      while (sa != a && *(sa-1) != '.')
	sa--;
      
      while (sb != b && *(sb-1) != '.')
	sb--;

      ca = sa;
      cb = sb;

      while (1) 
	{
	  if (ca == ea)
	    {
	      if (cb == eb)
		break;
	      
	      return -1;
	    }
	  
	  if (cb == eb)
	    return 1;
	  
	  ac = (unsigned char) *ca++;
	  bc = (unsigned char) *cb++;
	  
	  if (ac >= 'A' && ac <= 'Z')
	    ac += 'a' - 'A';
	  if (bc >= 'A' && bc <= 'Z')
	    bc += 'a' - 'A';
	  
	  if (ac < bc)
	    return -1;
	  else if (ac != bc)
	    return 1;
	}

     
      if (sa == a)
	{
	  if (sb == b)
	    return 0;
	  
	  return -1;
	}
      
      if (sb == b)
	return 1;
      
      ea = --sa;
      eb = --sb;
    }
}

static int prove_non_existence_nsec(struct dns_header *header, size_t plen, unsigned char **nsecs, unsigned char **labels, int nsec_count,
				    char *workspace1_in, char *workspace2, char *name, int type, int *nons)
{
  int i, rc, rdlen;
  unsigned char *p, *psave;
  int offset = (type & 0xff) >> 3;
  int mask = 0x80 >> (type & 0x07);

  if (nons)
    *nons = 1;
  
  /* Find NSEC record that proves name doesn't exist */
  for (i = 0; i < nsec_count; i++)
    {
      char *workspace1 = workspace1_in;
      int sig_labels, name_labels;

      p = nsecs[i];
      if (!extract_name(header, plen, &p, workspace1, 1, 10))
	return 0;
      p += 8; /* class, type, TTL */
      GETSHORT(rdlen, p);
      psave = p;
      if (!extract_name(header, plen, &p, workspace2, 1, 10))
	return 0;

      /* If NSEC comes from wildcard expansion, use original wildcard
	 as name for computation. */
      sig_labels = *labels[i];
      name_labels = count_labels(workspace1);

      if (sig_labels < name_labels)
	{
	  int k;
	  for (k = name_labels - sig_labels; k != 0; k--)
	    {
	      while (*workspace1 != '.' && *workspace1 != 0)
		workspace1++;
	      if (k != 1 && *workspace1 == '.')
		workspace1++;
	    }
	  
	  workspace1--;
	  *workspace1 = '*';
	}
	  
      rc = hostname_cmp(workspace1, name);
      
      if (rc == 0)
	{
	  /* 4035 para 5.4. Last sentence */
	  if (type == T_NSEC || type == T_RRSIG)
	    return 1;

	  /* NSEC with the same name as the RR we're testing, check
	     that the type in question doesn't appear in the type map */
	  rdlen -= p - psave;
	  /* rdlen is now length of type map, and p points to it */
	  
	  /* If we can prove that there's no NS record, return that information. */
	  if (nons && rdlen >= 2 && p[0] == 0 && (p[2] & (0x80 >> T_NS)) != 0)
	    *nons = 0;
	  
	  if (rdlen >= 2 && p[0] == 0)
	    {
	      /* A CNAME answer would also be valid, so if there's a CNAME is should 
		 have been returned. */
	      if ((p[2] & (0x80 >> T_CNAME)) != 0)
		return 0;
	      
	      /* If the SOA bit is set for a DS record, then we have the
		 DS from the wrong side of the delegation. For the root DS, 
		 this is expected. */
	      if (name_labels != 0 && type == T_DS && (p[2] & (0x80 >> T_SOA)) != 0)
		return 0;
	    }

	  while (rdlen >= 2)
	    {
	      if (!CHECK_LEN(header, p, plen, rdlen))
		return 0;
	      
	      if (p[0] == type >> 8)
		{
		  /* Does the NSEC say our type exists? */
		  if (offset < p[1] && (p[offset+2] & mask) != 0)
		    return 0;
		  
		  break; /* finished checking */
		}
	      
	      rdlen -= p[1];
	      p +=  p[1];
	    }
	  
	  return 1;
	}
      else if (rc == -1)
	{
	  /* Normal case, name falls between NSEC name and next domain name,
	     wrap around case, name falls between NSEC name (rc == -1) and end */
	  if (hostname_cmp(workspace2, name) >= 0 || hostname_cmp(workspace1, workspace2) >= 0)
	    return 1;
	}
      else 
	{
	  /* wrap around case, name falls between start and next domain name */
	  if (hostname_cmp(workspace1, workspace2) >= 0 && hostname_cmp(workspace2, name) >=0 )
	    return 1;
	}
    }
  
  return 0;
}

/* return digest length, or zero on error */
static int hash_name(char *in, unsigned char **out, const void *hash, 
		     unsigned char *salt, int salt_len, int iterations)
{
  void *ctx;
  unsigned char *digest;
  int i;

  if (!hash_init(hash, &ctx, &digest))
    return 0;
 
  hash_update(hash, ctx, to_wire(in), (unsigned char *)in);
  hash_update(hash, ctx, salt_len, salt);
  hash_digest(hash, ctx, hash_length(hash), digest);

  for(i = 0; i < iterations; i++)
    {
      hash_update(hash, ctx, hash_length(hash), digest);
      hash_update(hash, ctx, salt_len, salt);
      hash_digest(hash, ctx, hash_length(hash), digest);
    }
   
  from_wire(in);

  *out = digest;
  return hash_length(hash);
}

/* Decode base32 to first "." or end of string */
static int base32_decode(char *in, unsigned char *out)
{
  int oc, on, c, mask, i;
  unsigned char *p = out;
 
  for (c = *in, oc = 0, on = 0; c != 0 && c != '.'; c = *++in) 
    {
      if (c >= '0' && c <= '9')
	c -= '0';
      else if (c >= 'a' && c <= 'v')
	c -= 'a', c += 10;
      else if (c >= 'A' && c <= 'V')
	c -= 'A', c += 10;
      else
	return 0;
      
      for (mask = 0x10, i = 0; i < 5; i++)
        {
	  if (c & mask)
	    oc |= 1;
	  mask = mask >> 1;
	  if (((++on) & 7) == 0)
	    *p++ = oc;
	  oc = oc << 1;
	}
    }
  
  if ((on & 7) != 0)
    return 0;

  return p - out;
}

static int check_nsec3_coverage(struct dns_header *header, size_t plen, int digest_len, unsigned char *digest, int type,
				char *workspace1, char *workspace2, unsigned char **nsecs, int nsec_count, int *nons, int name_labels)
{
  int i, hash_len, salt_len, base32_len, rdlen, flags;
  unsigned char *p, *psave;

  for (i = 0; i < nsec_count; i++)
    if ((p = nsecs[i]))
      {
       	if (!extract_name(header, plen, &p, workspace1, 1, 0) ||
	    !(base32_len = base32_decode(workspace1, (unsigned char *)workspace2)))
	  return 0;
	
	p += 8; /* class, type, TTL */
	GETSHORT(rdlen, p);
	psave = p;
	p++; /* algo */
	flags = *p++; /* flags */
	p += 2; /* iterations */
	salt_len = *p++; /* salt_len */
	p += salt_len; /* salt */
	hash_len = *p++; /* p now points to next hashed name */
	
	if (!CHECK_LEN(header, p, plen, hash_len))
	  return 0;
	
	if (digest_len == base32_len && hash_len == base32_len)
	  {
	    int rc = memcmp(workspace2, digest, digest_len);

	    if (rc == 0)
	      {
		/* We found an NSEC3 whose hashed name exactly matches the query, so
		   we just need to check the type map. p points to the RR data for the record. */
		
		int offset = (type & 0xff) >> 3;
		int mask = 0x80 >> (type & 0x07);
		
		p += hash_len; /* skip next-domain hash */
		rdlen -= p - psave;

		if (!CHECK_LEN(header, p, plen, rdlen))
		  return 0;
		
		if (rdlen >= 2 && p[0] == 0)
		  {
		    /* If we can prove that there's no NS record, return that information. */
		    if (nons && (p[2] & (0x80 >> T_NS)) != 0)
		      *nons = 0;
		
		    /* A CNAME answer would also be valid, so if there's a CNAME is should 
		       have been returned. */
		    if ((p[2] & (0x80 >> T_CNAME)) != 0)
		      return 0;
		    
		    /* If the SOA bit is set for a DS record, then we have the
		       DS from the wrong side of the delegation. For the root DS, 
		       this is expected.  */
		    if (name_labels != 0 && type == T_DS && (p[2] & (0x80 >> T_SOA)) != 0)
		      return 0;
		  }

		while (rdlen >= 2)
		  {
		    if (p[0] == type >> 8)
		      {
			/* Does the NSEC3 say our type exists? */
			if (offset < p[1] && (p[offset+2] & mask) != 0)
			  return 0;
			
			break; /* finished checking */
		      }
		    
		    rdlen -= p[1];
		    p +=  p[1];
		  }
		
		return 1;
	      }
	    else if (rc < 0)
	      {
		/* Normal case, hash falls between NSEC3 name-hash and next domain name-hash,
		   wrap around case, name-hash falls between NSEC3 name-hash and end */
		if (memcmp(p, digest, digest_len) >= 0 || memcmp(workspace2, p, digest_len) >= 0)
		  {
		    if ((flags & 0x01) && nons) /* opt out */
		      *nons = 0;

		    return 1;
		  }
	      }
	    else 
	      {
		/* wrap around case, name falls between start and next domain name */
		if (memcmp(workspace2, p, digest_len) >= 0 && memcmp(p, digest, digest_len) >= 0)
		  {
		    if ((flags & 0x01) && nons) /* opt out */
		      *nons = 0;

		    return 1;
		  }
	      }
	  }
      }

  return 0;
}

static int prove_non_existence_nsec3(struct dns_header *header, size_t plen, unsigned char **nsecs, int nsec_count,
				     char *workspace1, char *workspace2, char *name, int type, char *wildname, int *nons)
{
  unsigned char *salt, *p, *digest;
  int digest_len, i, iterations, salt_len, base32_len, algo = 0;
  const void *hash;
  char *closest_encloser, *next_closest, *wildcard;
  
  if (nons)
    *nons = 1;
  
  /* Look though the NSEC3 records to find the first one with 
     an algorithm we support.

     Take the algo, iterations, and salt of that record
     as the ones we're going to use, and prune any 
     that don't match. */
  
  for (i = 0; i < nsec_count; i++)
    {
      if (!(p = skip_name(nsecs[i], header, plen, 15)))
	return 0; /* bad packet */
      
      p += 10; /* type, class, TTL, rdlen */
      algo = *p++;
      
      if ((hash = hash_find(nsec3_digest_name(algo))))
	break; /* known algo */
    }

  /* No usable NSEC3s */
  if (i == nsec_count)
    return 0;

  p++; /* flags */

  GETSHORT (iterations, p);
  /* Upper-bound iterations, to avoid DoS.
     Strictly, there are lower bounds for small keys, but
     since we don't have key size info here, at least limit
     to the largest bound, for 4096-bit keys. RFC 5155 10.3 */
  if (iterations > 2500)
    return 0;
  
  salt_len = *p++;
  salt = p;
  if (!CHECK_LEN(header, salt, plen, salt_len))
    return 0; /* bad packet */
    
  /* Now prune so we only have NSEC3 records with same iterations, salt and algo */
  for (i = 0; i < nsec_count; i++)
    {
      unsigned char *nsec3p = nsecs[i];
      int this_iter, flags;

      nsecs[i] = NULL; /* Speculative, will be restored if OK. */
      
      if (!(p = skip_name(nsec3p, header, plen, 15)))
	return 0; /* bad packet */
      
      p += 10; /* type, class, TTL, rdlen */
      
      if (*p++ != algo)
	continue;
 
      flags = *p++; /* flags */
      
      /* 5155 8.2 */
      if (flags != 0 && flags != 1)
	continue;

      GETSHORT(this_iter, p);
      if (this_iter != iterations)
	continue;

      if (salt_len != *p++)
	continue;
      
      if (!CHECK_LEN(header, p, plen, salt_len))
	return 0; /* bad packet */

      if (memcmp(p, salt, salt_len) != 0)
	continue;

      /* All match, put the pointer back */
      nsecs[i] = nsec3p;
    }

  if ((digest_len = hash_name(name, &digest, hash, salt, salt_len, iterations)) == 0)
    return 0;
  
  if (check_nsec3_coverage(header, plen, digest_len, digest, type, workspace1, workspace2, nsecs, nsec_count, nons, count_labels(name)))
    return 1;

  /* Can't find an NSEC3 which covers the name directly, we need the "closest encloser NSEC3" 
     or an answer inferred from a wildcard record. */
  closest_encloser = name;
  next_closest = NULL;

  do
    {
      if (*closest_encloser == '.')
	closest_encloser++;

      if (wildname && hostname_isequal(closest_encloser, wildname))
	break;

      if ((digest_len = hash_name(closest_encloser, &digest, hash, salt, salt_len, iterations)) == 0)
	return 0;
      
      for (i = 0; i < nsec_count; i++)
	if ((p = nsecs[i]))
	  {
	    if (!extract_name(header, plen, &p, workspace1, 1, 0) ||
		!(base32_len = base32_decode(workspace1, (unsigned char *)workspace2)))
	      return 0;
	  
	    if (digest_len == base32_len &&
		memcmp(digest, workspace2, digest_len) == 0)
	      break; /* Gotit */
	  }
      
      if (i != nsec_count)
	break;
      
      next_closest = closest_encloser;
    }
  while ((closest_encloser = strchr(closest_encloser, '.')));
  
  if (!closest_encloser || !next_closest)
    return 0;
  
  /* Look for NSEC3 that proves the non-existence of the next-closest encloser */
  if ((digest_len = hash_name(next_closest, &digest, hash, salt, salt_len, iterations)) == 0)
    return 0;

  if (!check_nsec3_coverage(header, plen, digest_len, digest, type, workspace1, workspace2, nsecs, nsec_count, NULL, 1))
    return 0;
  
  /* Finally, check that there's no seat of wildcard synthesis */
  if (!wildname)
    {
      if (!(wildcard = strchr(next_closest, '.')) || wildcard == next_closest)
	return 0;
      
      wildcard--;
      *wildcard = '*';
      
      if ((digest_len = hash_name(wildcard, &digest, hash, salt, salt_len, iterations)) == 0)
	return 0;
      
      if (!check_nsec3_coverage(header, plen, digest_len, digest, type, workspace1, workspace2, nsecs, nsec_count, NULL, 1))
	return 0;
    }
  
  return 1;
}

static int prove_non_existence(struct dns_header *header, size_t plen, char *keyname, char *name, int qtype, int qclass, char *wildname, int *nons, int *nsec_ttl)
{
  static unsigned char **nsecset = NULL, **rrsig_labels = NULL;
  static int nsecset_sz = 0, rrsig_labels_sz = 0;
  
  int type_found = 0;
  unsigned char *auth_start, *p = skip_questions(header, plen);
  int type, class, rdlen, i, nsecs_found;
  unsigned long ttl;
  
  /* Move to NS section */
  if (!p || !(p = skip_section(p, ntohs(header->ancount), header, plen)))
    return 0;

  auth_start = p;
  
  for (nsecs_found = 0, i = 0; i < ntohs(header->nscount); i++)
    {
      unsigned char *pstart = p;
      
      if (!extract_name(header, plen, &p, daemon->workspacename, 1, 10))
	return 0;
	  
      GETSHORT(type, p); 
      GETSHORT(class, p);
      GETLONG(ttl, p);
      GETSHORT(rdlen, p);

      if (class == qclass && (type == T_NSEC || type == T_NSEC3))
	{
	  if (nsec_ttl)
	    {
	      /* Limit TTL with sig TTL */
	      if (daemon->rr_status[ntohs(header->ancount) + i] < ttl)
		ttl = daemon->rr_status[ntohs(header->ancount) + i];
	      *nsec_ttl = ttl;
	    }
	  
	  /* No mixed NSECing 'round here, thankyouverymuch */
	  if (type_found != 0 && type_found != type)
	    return 0;

	  type_found = type;

	  if (!expand_workspace(&nsecset, &nsecset_sz, nsecs_found))
	    return 0; 
	  
	  if (type == T_NSEC)
	    {
	      /* If we're looking for NSECs, find the corresponding SIGs, to 
		 extract the labels value, which we need in case the NSECs
		 are the result of wildcard expansion.
		 Note that the NSEC may not have been validated yet
		 so if there are multiple SIGs, make sure the label value
		 is the same in all, to avoid be duped by a rogue one.
		 If there are no SIGs, that's an error */
	      unsigned char *p1 = auth_start;
	      int res, j, rdlen1, type1, class1;
	      
	      if (!expand_workspace(&rrsig_labels, &rrsig_labels_sz, nsecs_found))
		return 0;
	      
	      rrsig_labels[nsecs_found] = NULL;
	      
	      for (j = ntohs(header->nscount); j != 0; j--)
		{
		  if (!(res = extract_name(header, plen, &p1, daemon->workspacename, 0, 10)))
		    return 0;

		   GETSHORT(type1, p1); 
		   GETSHORT(class1, p1);
		   p1 += 4; /* TTL */
		   GETSHORT(rdlen1, p1);

		   if (!CHECK_LEN(header, p1, plen, rdlen1))
		     return 0;
		   
		   if (res == 1 && class1 == qclass && type1 == T_RRSIG)
		     {
		       int type_covered;
		       unsigned char *psav = p1;
		       
		       if (rdlen1 < 18)
			 return 0; /* bad packet */

		       GETSHORT(type_covered, p1);

		       if (type_covered == T_NSEC)
			 {
			   p1++; /* algo */
			   
			   /* labels field must be the same in every SIG we find. */
			   if (!rrsig_labels[nsecs_found])
			     rrsig_labels[nsecs_found] = p1;
			   else if (*rrsig_labels[nsecs_found] != *p1) /* algo */
			     return 0;
			   }
		       p1 = psav;
		     }
		   
		   if (!ADD_RDLEN(header, p1, plen, rdlen1))
		     return 0;
		}

	      /* Must have found at least one sig. */
	      if (!rrsig_labels[nsecs_found])
		return 0;
	    }

	  nsecset[nsecs_found++] = pstart;   
	}
      
      if (!ADD_RDLEN(header, p, plen, rdlen))
	return 0;
    }
  
  if (type_found == T_NSEC)
    return prove_non_existence_nsec(header, plen, nsecset, rrsig_labels, nsecs_found, daemon->workspacename, keyname, name, qtype, nons);
  else if (type_found == T_NSEC3)
    return prove_non_existence_nsec3(header, plen, nsecset, nsecs_found, daemon->workspacename, keyname, name, qtype, wildname, nons);
  else
    return 0;
}

/* Check signing status of name.
   returns:
   STAT_SECURE   zone is signed.
   STAT_INSECURE zone proved unsigned.
   STAT_NEED_DS  require DS record of name returned in keyname.
   STAT_NEED_KEY require DNSKEY record of name returned in keyname.
   name returned unaltered.
*/
static int zone_status(char *name, int class, char *keyname, time_t now)
{
  int name_start = strlen(name); /* for when TA is root */
  struct crec *crecp;
  char *p;

  /* First, work towards the root, looking for a trust anchor.
     This can either be one configured, or one previously cached.
     We can assume, if we don't find one first, that there is
     a trust anchor at the root. */
  for (p = name; p; p = strchr(p, '.'))
    {
      if (*p == '.')
	p++;

      if (cache_find_by_name(NULL, p, now, F_DS))
	{
	  name_start = p - name;
	  break;
	}
    }

  /* Now work away from the trust anchor */
  while (1)
    {
      strcpy(keyname, &name[name_start]);
      
      if (!(crecp = cache_find_by_name(NULL, keyname, now, F_DS)))
	return STAT_NEED_DS;
      
       /* F_DNSSECOK misused in DS cache records to non-existence of NS record.
	  F_NEG && !F_DNSSECOK implies that we've proved there's no DS record here,
	  but that's because there's no NS record either, ie this isn't the start
	  of a zone. We only prove that the DNS tree below a node is unsigned when
	  we prove that we're at a zone cut AND there's no DS record. */
      if (crecp->flags & F_NEG)
	{
	  if (crecp->flags & F_DNSSECOK)
	    return STAT_INSECURE; /* proved no DS here */
	}
      else
	{
	  /* If all the DS records have digest and/or sig algos we don't support,
	     then the zone is insecure. Note that if an algo
	     appears in the DS, then RRSIGs for that algo MUST
	     exist for each RRset: 4035 para 2.2  So if we find
	     a DS here with digest and sig we can do, we're entitled
	     to assume we can validate the zone and if we can't later,
	     because an RRSIG is missing we return BOGUS.
	  */
	  do 
	    {
	      if (crecp->uid == (unsigned int)class &&
		  ds_digest_name(crecp->addr.ds.digest) &&
		  algo_digest_name(crecp->addr.ds.algo))
		break;
	    }
	  while ((crecp = cache_find_by_name(crecp, keyname, now, F_DS)));

	  if (!crecp)
	    return STAT_INSECURE;
	}

      if (name_start == 0)
	break;

      for (p = &name[name_start-2]; (*p != '.') && (p != name); p--);
      
      if (p != name)
        p++;
      
      name_start = p - name;
    } 

  return STAT_SECURE;
}
       
/* Validate all the RRsets in the answer and authority sections of the reply (4035:3.2.3) 
   Return code:
   STAT_SECURE   if it validates.
   STAT_INSECURE at least one RRset not validated, because in unsigned zone.
   STAT_BOGUS    signature is wrong, bad packet, no validation where there should be.
   STAT_NEED_KEY need DNSKEY to complete validation (name is returned in keyname, class in *class)
   STAT_NEED_DS  need DS to complete validation (name is returned in keyname)

   daemon->rr_status points to a char array which corressponds to the RRs in the 
   answer and auth sections. This is set to 1 for each RR which is validated, and 0 for any which aren't.

   When validating replies to DS records, we're only interested in the NSEC{3} RRs in the auth section.
   Other RRs in that section missing sigs will not cause am INSECURE reply. We determine this mode
   is the nons argument is non-NULL.
*/
int dnssec_validate_reply(time_t now, struct dns_header *header, size_t plen, char *name, char *keyname, 
			  int *class, int check_unsigned, int *neganswer, int *nons, int *nsec_ttl)
{
  static unsigned char **targets = NULL;
  static int target_sz = 0;

  unsigned char *ans_start, *p1, *p2;
  int type1, class1, rdlen1 = 0, type2, class2, rdlen2, qclass, qtype, targetidx;
  int i, j, rc = STAT_INSECURE;
  int secure = STAT_SECURE;

  /* extend rr_status if necessary */
  if (daemon->rr_status_sz < ntohs(header->ancount) + ntohs(header->nscount))
    {
      unsigned long *new = whine_malloc(sizeof(*daemon->rr_status) * (ntohs(header->ancount) + ntohs(header->nscount) + 64));

      if (!new)
	return STAT_BOGUS;

      free(daemon->rr_status);
      daemon->rr_status = new;
      daemon->rr_status_sz = ntohs(header->ancount) + ntohs(header->nscount) + 64;
    }
  
  memset(daemon->rr_status, 0, sizeof(*daemon->rr_status) * daemon->rr_status_sz);
  
  if (neganswer)
    *neganswer = 0;
  
  if (RCODE(header) == SERVFAIL || ntohs(header->qdcount) != 1)
    return STAT_BOGUS;
  
  if (RCODE(header) != NXDOMAIN && RCODE(header) != NOERROR)
    return STAT_INSECURE;

  p1 = (unsigned char *)(header+1);
  
   /* Find all the targets we're looking for answers to.
     The zeroth array element is for the query, subsequent ones
     for CNAME targets, unless the query is for a CNAME. */

  if (!expand_workspace(&targets, &target_sz, 0))
    return STAT_BOGUS;
  
  targets[0] = p1;
  targetidx = 1;
   
  if (!extract_name(header, plen, &p1, name, 1, 4))
    return STAT_BOGUS;
  
  GETSHORT(qtype, p1);
  GETSHORT(qclass, p1);
  ans_start = p1;
 
  /* Can't validate an RRSIG query */
  if (qtype == T_RRSIG)
    return STAT_INSECURE;
  
  if (qtype != T_CNAME)
    for (j = ntohs(header->ancount); j != 0; j--) 
      {
	if (!(p1 = skip_name(p1, header, plen, 10)))
	  return STAT_BOGUS; /* bad packet */
	
	GETSHORT(type2, p1); 
	p1 += 6; /* class, TTL */
	GETSHORT(rdlen2, p1);  
	
	if (type2 == T_CNAME)
	  {
	    if (!expand_workspace(&targets, &target_sz, targetidx))
	      return STAT_BOGUS;
	    
	    targets[targetidx++] = p1; /* pointer to target name */
	  }
	
	if (!ADD_RDLEN(header, p1, plen, rdlen2))
	  return STAT_BOGUS;
      }
  
  for (p1 = ans_start, i = 0; i < ntohs(header->ancount) + ntohs(header->nscount); i++)
    {
      if (i != 0 && !ADD_RDLEN(header, p1, plen, rdlen1))
	return STAT_BOGUS;
      
      if (!extract_name(header, plen, &p1, name, 1, 10))
	return STAT_BOGUS; /* bad packet */
      
      GETSHORT(type1, p1);
      GETSHORT(class1, p1);
      p1 += 4; /* TTL */
      GETSHORT(rdlen1, p1);
      
      /* Don't try and validate RRSIGs! */
      if (type1 == T_RRSIG)
	continue;
      
      /* Check if we've done this RRset already */
      for (p2 = ans_start, j = 0; j < i; j++)
	{
	  if (!(rc = extract_name(header, plen, &p2, name, 0, 10)))
	    return STAT_BOGUS; /* bad packet */
	  
	  GETSHORT(type2, p2);
	  GETSHORT(class2, p2);
	  p2 += 4; /* TTL */
	  GETSHORT(rdlen2, p2);
	  
	  if (type2 == type1 && class2 == class1 && rc == 1)
	    break; /* Done it before: name, type, class all match. */
	  
	  if (!ADD_RDLEN(header, p2, plen, rdlen2))
	    return STAT_BOGUS;
	}
      
      /* Done already: copy the validation status */
      if (j != i)
	daemon->rr_status[i] = daemon->rr_status[j];
      else
	{
	  /* Not done, validate now */
	  int sigcnt, rrcnt;
	  char *wildname;
	  
	  if (!explore_rrset(header, plen, class1, type1, name, keyname, &sigcnt, &rrcnt))
	    return STAT_BOGUS;
	  
	  /* No signatures for RRset. We can be configured to assume this is OK and return an INSECURE result. */
	  if (sigcnt == 0)
	    {
	      /* NSEC and NSEC3 records must be signed. We make this assumption elsewhere. */
	      if (type1 == T_NSEC || type1 == T_NSEC3)
		rc = STAT_INSECURE;
	      else if (nons && i >= ntohs(header->ancount))
		/* If we're validating a DS reply, rather than looking for the value of AD bit,
		   we only care that NSEC and NSEC3 RRs in the auth section are signed. 
		   Return SECURE even if others (SOA....) are not. */
		rc = STAT_SECURE;
	      else
		{
		  /* unsigned RRsets in auth section are not BOGUS, but do make reply insecure. */
		  if (check_unsigned && i < ntohs(header->ancount))
		    {
		      rc = zone_status(name, class1, keyname, now);
		      if (rc == STAT_SECURE)
			rc = STAT_BOGUS;
		      if (class)
			*class = class1; /* Class for NEED_DS or NEED_KEY */
		    }
		  else 
		    rc = STAT_INSECURE; 
		  
		  if (rc != STAT_INSECURE)
		    return rc;
		}
	    }
	  else
	    {
	      /* explore_rrset() gives us key name from sigs in keyname.
		 Can't overwrite name here. */
	      strcpy(daemon->workspacename, keyname);
	      rc = zone_status(daemon->workspacename, class1, keyname, now);
	      
	      if (rc == STAT_BOGUS || rc == STAT_NEED_KEY || rc == STAT_NEED_DS)
		{
		  if (class)
		    *class = class1; /* Class for NEED_DS or NEED_KEY */
		  return rc;
		}
	      
	      /* Zone is insecure, don't need to validate RRset */
	      if (rc == STAT_SECURE)
		{
		  unsigned long sig_ttl;
		  rc = validate_rrset(now, header, plen, class1, type1, sigcnt,
				      rrcnt, name, keyname, &wildname, NULL, 0, 0, 0, &sig_ttl);
		  
		  if (rc == STAT_BOGUS || rc == STAT_NEED_KEY || rc == STAT_NEED_DS)
		    {
		      if (class)
			*class = class1; /* Class for DS or DNSKEY */
		      return rc;
		    } 
		  
		  /* rc is now STAT_SECURE or STAT_SECURE_WILDCARD */
		  
		  /* Note that RR is validated */
		  daemon->rr_status[i] = sig_ttl;
		   
		  /* Note if we've validated either the answer to the question
		     or the target of a CNAME. Any not noted will need NSEC or
		     to be in unsigned space. */
		  for (j = 0; j <targetidx; j++)
		    if ((p2 = targets[j]))
		      {
			int rc1;
			if (!(rc1 = extract_name(header, plen, &p2, name, 0, 10)))
			  return STAT_BOGUS; /* bad packet */
			
			if (class1 == qclass && rc1 == 1 && (type1 == T_CNAME || type1 == qtype || qtype == T_ANY ))
			  targets[j] = NULL;
		      }
		  
		  /* An attacker replay a wildcard answer with a different
		     answer and overlay a genuine RR. To prove this
		     hasn't happened, the answer must prove that
		     the genuine record doesn't exist. Check that here. 
		     Note that we may not yet have validated the NSEC/NSEC3 RRsets. 
		     That's not a problem since if the RRsets later fail
		     we'll return BOGUS then. */
		  if (rc == STAT_SECURE_WILDCARD &&
		      !prove_non_existence(header, plen, keyname, name, type1, class1, wildname, NULL, NULL))
		    return STAT_BOGUS;

		  rc = STAT_SECURE;
		}
	    }
	}

      if (rc == STAT_INSECURE)
	secure = STAT_INSECURE;
    }

  /* OK, all the RRsets validate, now see if we have a missing answer or CNAME target. */
  if (secure == STAT_SECURE)
    for (j = 0; j <targetidx; j++)
      if ((p2 = targets[j]))
	{
	  if (neganswer)
	    *neganswer = 1;
	  
	  if (!extract_name(header, plen, &p2, name, 1, 10))
	    return STAT_BOGUS; /* bad packet */
	  
	  /* NXDOMAIN or NODATA reply, unanswered question is (name, qclass, qtype) */
	  
	  /* For anything other than a DS record, this situation is OK if either
	     the answer is in an unsigned zone, or there's a NSEC records. */
	  if (!prove_non_existence(header, plen, keyname, name, qtype, qclass, NULL, nons, nsec_ttl))
	    {
	      /* Empty DS without NSECS */
	      if (qtype == T_DS)
		return STAT_BOGUS;
	      
	      if ((rc = zone_status(name, qclass, keyname, now)) != STAT_SECURE)
		{
		  if (class)
		    *class = qclass; /* Class for NEED_DS or NEED_KEY */
		  return rc;
		} 
	      
	      return STAT_BOGUS; /* signed zone, no NSECs */
	    }
	}
  
  return secure;
}


/* Compute keytag (checksum to quickly index a key). See RFC4034 */
int dnskey_keytag(int alg, int flags, unsigned char *key, int keylen)
{
  if (alg == 1)
    {
      /* Algorithm 1 (RSAMD5) has a different (older) keytag calculation algorithm.
         See RFC4034, Appendix B.1 */
      return key[keylen-4] * 256 + key[keylen-3];
    }
  else
    {
      unsigned long ac = flags + 0x300 + alg;
      int i;

      for (i = 0; i < keylen; ++i)
        ac += (i & 1) ? key[i] : key[i] << 8;

      ac += (ac >> 16) & 0xffff;
      return ac & 0xffff;
    }
}

size_t dnssec_generate_query(struct dns_header *header, unsigned char *end, char *name, int class, 
			     int type, int edns_pktsz)
{
  unsigned char *p;
  size_t ret;

  header->qdcount = htons(1);
  header->ancount = htons(0);
  header->nscount = htons(0);
  header->arcount = htons(0);

  header->hb3 = HB3_RD; 
  SET_OPCODE(header, QUERY);
  /* For debugging, set Checking Disabled, otherwise, have the upstream check too,
     this allows it to select auth servers when one is returning bad data. */
  header->hb4 = option_bool(OPT_DNSSEC_DEBUG) ? HB4_CD : 0;

  /* ID filled in later */

  p = (unsigned char *)(header+1);
	
  p = do_rfc1035_name(p, name, NULL);
  *p++ = 0;
  PUTSHORT(type, p);
  PUTSHORT(class, p);

  ret = add_do_bit(header, p - (unsigned char *)header, end);

  if (find_pseudoheader(header, ret, NULL, &p, NULL, NULL))
    PUTSHORT(edns_pktsz, p);

  return ret;
}

unsigned char* hash_questions(struct dns_header *header, size_t plen, char *name)
{
  int q;
  unsigned int len;
  unsigned char *p = (unsigned char *)(header+1);
  const void *hash;
  void *ctx;
  unsigned char *digest;
  
  if (!(hash = hash_find(HASH_NAME)) || !hash_init(hash, &ctx, &digest))
    return NULL;
  
  for (q = ntohs(header->qdcount); q != 0; q--) 
    {
      if (!extract_name(header, plen, &p, name, 1, 4))
	break; /* bad packet */
      
      len = to_wire(name);
      hash_update(hash, ctx, len, (unsigned char *)name);
      /* CRC the class and type as well */
      hash_update(hash, ctx, 4, p);

      p += 4;
      if (!CHECK_LEN(header, p, plen, 0))
	break; /* bad packet */
    }
  
  hash_digest(hash, ctx, hash_length(hash), digest);
  return digest;
}

#endif /* HAVE_DNSSEC */
