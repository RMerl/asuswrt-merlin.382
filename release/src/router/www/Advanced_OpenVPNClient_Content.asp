<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
<html xmlns="http://www.w3.org/1999/xhtml">
<html xmlns:v>
<head>
<meta http-equiv="X-UA-Compatible" content="IE=Edge"/>
<meta http-equiv="Content-Type" content="text/html; charset=utf-8" />
<meta HTTP-EQUIV="Pragma" CONTENT="no-cache">
<meta HTTP-EQUIV="Expires" CONTENT="-1">
<link rel="shortcut icon" href="images/favicon.png">
<link rel="icon" href="images/favicon.png">
<title><#Web_Title#> - OpenVPN Client Settings</title>
<link rel="stylesheet" type="text/css" href="index_style.css">
<link rel="stylesheet" type="text/css" href="form_style.css">
<script language="JavaScript" type="text/javascript" src="/state.js"></script>
<script language="JavaScript" type="text/javascript" src="/general.js"></script>
<script language="JavaScript" type="text/javascript" src="/popup.js"></script>
<script language="JavaScript" type="text/javascript" src="/help.js"></script>
<script type="text/javascript" src="/validator.js"></script>
<script language="JavaScript" type="text/javascript" src="/client_function.js"></script>
<script type="text/javascript" language="JavaScript" src="/base64.js"></script>

<script type="text/javascript" src="/js/jquery.js"></script>
<script type="text/javascript" src="/switcherplugin/jquery.iphone-switch.js"></script>
<script type="text/javascript" src="/js/httpApi.js"></script>
<style type="text/css">
.contentM_qis{
	width:740px;
	margin-top:220px;
	margin-left:380px;
	position:absolute;
	-webkit-border-radius: 5px;
	-moz-border-radius: 5px;
	border-radius: 5px;
	z-index:200;
	background-color:#2B373B;
	display:none;
	/*behavior: url(/PIE.htc);*/
}
.QISmain{
	width:730px;
	/*font-family:Verdana, Arial, Helvetica, sans-serif;*/
	font-size:14px;
	z-index:200;
	position:relative;
	background-color:black;
}
.QISform_wireless{
	width:600px;
	font-size:12px;
	color:#FFFFFF;
	margin-top:10px;
	*margin-left:10px;
}

.QISform_wireless thead{
	font-size:15px;
	line-height:20px;
	color:#FFFFFF;
}

.QISform_wireless th{
	padding-left:10px;
	*padding-left:30px;
	font-size:12px;
	color: #FFFFFF;
	text-align:left;
}

.QISform_wireless li{
	margin-top:10px;
}
.QISGeneralFont{
	font-family:Segoe UI, Arial, sans-serif;
	margin-top:10px;
	margin-left:70px;
	*margin-left:50px;
	margin-right:30px;
	color:white;
	LINE-HEIGHT:18px;
}	
.description_down{
	margin-top:10px;
	margin-left:10px;
	padding-left:5px;
	font-weight:bold;
	line-height:140%;
	color:#ffffff;
}
#ClientList_Block_PC{
	border:1px outset #999;
	background-color:#576D73;
	position:absolute;
	*margin-top:26px;
	margin-left:53px;
	*margin-left:-189px;
	width:255px;
	text-align:left;
	height:auto;
	overflow-y:auto;
	z-index:200;
	padding: 1px;
	display:none;
}
#ClientList_Block_PC div{
	background-color:#576D73;
	height:auto;
	*height:20px;
	line-height:20px;
	text-decoration:none;
	font-family: Lucida Console;
	padding-left:2px;
}

#ClientList_Block_PC a{
	background-color:#EFEFEF;
	color:#FFF;
	font-size:12px;
	font-family:Arial, Helvetica, sans-serif;
	text-decoration:none;
}
#ClientList_Block_PC div:hover, #ClientList_Block a:hover{
	background-color:#3366FF;
	color:#FFFFFF;
	cursor:default;
}
</style>
<script>

wan_route_x = '<% nvram_get("wan_route_x"); %>';
wan_nat_x = '<% nvram_get("wan_nat_x"); %>';
wan_proto = '<% nvram_get("wan_proto"); %>';
<% vpn_client_get_parameter(); %>

openvpn_unit = '<% nvram_get("vpn_client_unit"); %>';

switch (openvpn_unit) {
	case "1":
		client_state = (<% sysinfo("pid.vpnclient1"); %> > 0 ? 2 : 0);
		break;
	case "2":
		client_state = (<% sysinfo("pid.vpnclient2"); %> > 0 ? 2 : 0);
		break;
	case "3":
		client_state = (<% sysinfo("pid.vpnclient3"); %> > 0 ? 2 : 0);
		break;
	case "4":
		client_state = (<% sysinfo("pid.vpnclient4"); %> > 0 ? 2 : 0);
		break;
	case "5":
		client_state = (<% sysinfo("pid.vpnclient5"); %> > 0 ? 2 : 0);
		break;
	default:
		client_state = 0;
		break;
}


enforce_ori = "<% nvram_get("vpn_client_enforce"); %>";
policy_ori = "<% nvram_get("vpn_client_rgw"); %>";
dnsmode_ori = "<% nvram_get("vpn_client_adns"); %>";

ciphersarray = [
		["AES-128-CBC"],
		["AES-192-CBC"],
		["AES-256-CBC"],
		["AES-128-GCM"],
		["AES-192-GCM"],
		["AES-256-GCM"],
		["BF-CBC"],
		["CAST5-CBC"],
		["DES-CBC"],
		["DES-EDE3-CBC"],
		["DES-EDE-CBC"],
		["DESX-CBC"],
		["IDEA-CBC"]
];

var digestsarray = [
		["DSA"],
		["DSA-SHA"],
		["DSA-SHA1"],
		["DSA-SHA1-old"],
		["ecdsa-with-SHA1"],
		["MD4"],
		["MD5"],
		["MDC2"],
		["RIPEMD160"],
		["RSA-MD4"],
		["RSA-MD5"],
		["RSA-MDC2"],
		["RSA-RIPEMD160"],
		["RSA-SHA"],
		["RSA-SHA1"],
		["RSA-SHA1-2"],
		["RSA-SHA224"],
		["RSA-SHA256"],
		["RSA-SHA384"],
		["RSA-SHA512"],
		["SHA"],
		["SHA1"],
		["SHA224"],
		["SHA256"],
		["SHA384"],
		["SHA512"],
		["whirlpool"]
];


var clientlist_array = "<% nvram_get("vpn_client_clientlist"); %>";
if (isSupport("hnd")) {
	clientlist_array += "<% nvram_get("vpn_client_clientlist1"); %>"+
			    "<% nvram_get("vpn_client_clientlist2"); %>"+
			    "<% nvram_get("vpn_client_clientlist3"); %>"+
			    "<% nvram_get("vpn_client_clientlist4"); %>"+
			    "<% nvram_get("vpn_client_clientlist5"); %>";
}

function initial()
{
	show_menu();

	if(vpnc_support) {
		var vpn_client_array = {"OpenVPN" : ["OpenVPN", "Advanced_OpenVPNClient_Content.asp"], "PPTP" : ["PPTP/L2TP", "Advanced_VPNClient_Content.asp"]};
		$('#divSwitchMenu').html(gen_switch_menu(vpn_client_array, "OpenVPN"));
		document.getElementById("divSwitchMenu").style.display = "";
	}

	showclientlist();
	showDropdownClientList('setClientIP', 'name>ip', 'all', 'ClientList_Block_PC', 'pull_arrow', 'online');

	// Client list
	free_options(document.form.vpn_client_unit);
	add_option(document.form.vpn_client_unit, "1: <% nvram_get("vpn_client1_desc"); %>", "1", (openvpn_unit == 1));
	add_option(document.form.vpn_client_unit, "2: <% nvram_get("vpn_client2_desc"); %>", "2", (openvpn_unit == 2));
	add_option(document.form.vpn_client_unit, "3: <% nvram_get("vpn_client3_desc"); %>", "3", (openvpn_unit == 3));
	add_option(document.form.vpn_client_unit, "4: <% nvram_get("vpn_client4_desc"); %>", "4", (openvpn_unit == 4));
	add_option(document.form.vpn_client_unit, "5: <% nvram_get("vpn_client5_desc"); %>", "5", (openvpn_unit == 5));

	// Cipher list
	free_options(document.form.vpn_client_cipher);
	currentcipher = "<% nvram_get("vpn_client_cipher"); %>";
	add_option(document.form.vpn_client_cipher, "Default","default",(currentcipher.toLowerCase() == "default"));
	add_option(document.form.vpn_client_cipher, "None","none",(currentcipher.toLowerCase() == "none"));

	// Digest list
	free_options(document.form.vpn_client_digest);
	currentdigest = "<% nvram_get("vpn_client_digest"); %>";
	add_option(document.form.vpn_client_digest, "Default","default",(currentdigest.toLowerCase() == "default"));
	add_option(document.form.vpn_client_digest, "None","none",(currentdigest.toLowerCase() == "none"));

	// Extract the type out of the interface name 
	// (imported ovpn can result in this being tun3, for example)
	currentiface = "<% nvram_get("vpn_client_if"); %>";
	add_option(document.form.vpn_client_if_x, "TUN","tun",(currentiface.indexOf("tun") != -1));
	add_option(document.form.vpn_client_if_x, "TAP","tap",(currentiface.indexOf("tap") != -1));

	for(var i = 0; i < ciphersarray.length; i++){
		add_option(document.form.vpn_client_cipher,
			ciphersarray[i][0], ciphersarray[i][0],
			(currentcipher.toLowerCase() == ciphersarray[i][0].toLowerCase()));
	}

	for(var i = 0; i < digestsarray.length; i++){
		add_option(document.form.vpn_client_digest,
			digestsarray[i][0], digestsarray[i][0],
			(currentdigest.toLowerCase() == digestsarray[i][0].toLowerCase()));
	}

	// Set these based on a compound field
	setRadioValue(document.form.vpn_client_x_eas, ((document.form.vpn_clientx_eas.value.indexOf(''+(openvpn_unit)) >= 0) ? "1" : "0"));

	getTLS(openvpn_unit);
	update_rgw_options();
	document.form.vpn_client_rgw.value = policy_ori;
	update_visibility();


	var cust2 = document.form.vpn_client_cust2.value;
	if (isSupport("hnd")) {
		document.getElementById("vpn_client_custom_x").maxLength = 170 * 3;     // 255 * 3 - base64 overhead
		cust2 += document.form.vpn_client_cust21.value +
		           document.form.vpn_client_cust22.value;
	}
	document.getElementById("vpn_client_custom_x").value = Base64.decode(cust2);

	setTimeout("getConnStatus()", 1000);

}

function getTLS(unit){
        $.ajax({
                url: '/ajax_openvpn_server.asp',
                dataType: 'script',
                timeout: 1500,
                error: function(xhr){
                        setTimeout("getTLS(" + unit + ");",1000);
                },
                success: function(){
                        setTLSTable(unit);
                }
        });
}

function setTLSTable(unit) {
	switch (unit) {
		case "1" :
			document.getElementById('edit_vpn_crt_client_ca').value = vpn_crt_client1_ca[0].replace(/&#10/g, "\n").replace(/&#13/g, "\r");
			document.getElementById('edit_vpn_crt_client_crt').value = vpn_crt_client1_crt[0].replace(/&#10/g, "\n").replace(/&#13/g, "\r");
			document.getElementById('edit_vpn_crt_client_key').value = vpn_crt_client1_key[0].replace(/&#10/g, "\n").replace(/&#13/g, "\r");
			document.getElementById('edit_vpn_crt_client_static').value = vpn_crt_client1_static[0].replace(/&#10/g, "\n").replace(/&#13/g, "\r");
			document.getElementById('edit_vpn_crt_client_crl').value = vpn_crt_client1_crl[0].replace(/&#10/g, "\n").replace(/&#13/g, "\r");
			document.getElementById('edit_vpn_crt_client_extra').value = vpn_crt_client1_extra[0].replace(/&#10/g, "\n").replace(/&#13/g, "\r");
			break;
		case "2" :
			document.getElementById('edit_vpn_crt_client_ca').value = vpn_crt_client2_ca[0].replace(/&#10/g, "\n").replace(/&#13/g, "\r");
			document.getElementById('edit_vpn_crt_client_crt').value = vpn_crt_client2_crt[0].replace(/&#10/g, "\n").replace(/&#13/g, "\r");
			document.getElementById('edit_vpn_crt_client_key').value = vpn_crt_client2_key[0].replace(/&#10/g, "\n").replace(/&#13/g, "\r");
			document.getElementById('edit_vpn_crt_client_static').value = vpn_crt_client2_static[0].replace(/&#10/g, "\n").replace(/&#13/g, "\r");
			document.getElementById('edit_vpn_crt_client_crl').value = vpn_crt_client2_crl[0].replace(/&#10/g, "\n").replace(/&#13/g, "\r");
			document.getElementById('edit_vpn_crt_client_extra').value = vpn_crt_client2_extra[0].replace(/&#10/g, "\n").replace(/&#13/g, "\r");
			break;
		case "3" :
			document.getElementById('edit_vpn_crt_client_ca').value = vpn_crt_client3_ca[0].replace(/&#10/g, "\n").replace(/&#13/g, "\r");
			document.getElementById('edit_vpn_crt_client_crt').value = vpn_crt_client3_crt[0].replace(/&#10/g, "\n").replace(/&#13/g, "\r");
			document.getElementById('edit_vpn_crt_client_key').value = vpn_crt_client3_key[0].replace(/&#10/g, "\n").replace(/&#13/g, "\r");
			document.getElementById('edit_vpn_crt_client_static').value = vpn_crt_client3_static[0].replace(/&#10/g, "\n").replace(/&#13/g, "\r");
			document.getElementById('edit_vpn_crt_client_crl').value = vpn_crt_client3_crl[0].replace(/&#10/g, "\n").replace(/&#13/g, "\r");
			document.getElementById('edit_vpn_crt_client_extra').value = vpn_crt_client3_extra[0].replace(/&#10/g, "\n").replace(/&#13/g, "\r");
			break;
		case "4" :
			document.getElementById('edit_vpn_crt_client_ca').value = vpn_crt_client4_ca[0].replace(/&#10/g, "\n").replace(/&#13/g, "\r");
			document.getElementById('edit_vpn_crt_client_crt').value = vpn_crt_client4_crt[0].replace(/&#10/g, "\n").replace(/&#13/g, "\r");
			document.getElementById('edit_vpn_crt_client_key').value = vpn_crt_client4_key[0].replace(/&#10/g, "\n").replace(/&#13/g, "\r");
			document.getElementById('edit_vpn_crt_client_static').value = vpn_crt_client4_static[0].replace(/&#10/g, "\n").replace(/&#13/g, "\r");
			document.getElementById('edit_vpn_crt_client_crl').value = vpn_crt_client4_crl[0].replace(/&#10/g, "\n").replace(/&#13/g, "\r");
			document.getElementById('edit_vpn_crt_client_extra').value = vpn_crt_client4_extra[0].replace(/&#10/g, "\n").replace(/&#13/g, "\r");
			break;
		case "5" :
			document.getElementById('edit_vpn_crt_client_ca').value = vpn_crt_client5_ca[0].replace(/&#10/g, "\n").replace(/&#13/g, "\r");
			document.getElementById('edit_vpn_crt_client_crt').value = vpn_crt_client5_crt[0].replace(/&#10/g, "\n").replace(/&#13/g, "\r");
			document.getElementById('edit_vpn_crt_client_key').value = vpn_crt_client5_key[0].replace(/&#10/g, "\n").replace(/&#13/g, "\r");
			document.getElementById('edit_vpn_crt_client_static').value = vpn_crt_client5_static[0].replace(/&#10/g, "\n").replace(/&#13/g, "\r");
			document.getElementById('edit_vpn_crt_client_crl').value = vpn_crt_client5_crl[0].replace(/&#10/g, "\n").replace(/&#13/g, "\r");
			document.getElementById('edit_vpn_crt_client_extra').value = vpn_crt_client5_extra[0].replace(/&#10/g, "\n").replace(/&#13/g, "\r");
			break;
	}
}


function update_visibility(){
	auth = document.form.vpn_client_crypt.value;
	iface = document.form.vpn_client_if_x.value;
	bridge = getRadioValue(document.form.vpn_client_bridge);
	nat = getRadioValue(document.form.vpn_client_nat);
	hmac = document.form.vpn_client_hmac.value;
	rgw = document.form.vpn_client_rgw.value;
	tlsremote = getRadioValue(document.form.vpn_client_tlsremote);
	userauth = (getRadioValue(document.form.vpn_client_userauth) == 1) && (auth == 'tls') ? 1 : 0;
	useronly = userauth && getRadioValue(document.form.vpn_client_useronly);
	ncp = document.form.vpn_client_ncp_enable.value;

	showhide("client_userauth", (auth == "tls"));
	showhide("client_hmac", (auth == "tls"));

	showhide("client_username", userauth);
	showhide("client_password", userauth);
	showhide("client_useronly", userauth);

	showhide("client_ca_warn_text", useronly);
	showhide("client_bridge", (iface == "tap"));

	showhide("client_bridge_warn_text", (bridge == 0));
	showhide("client_nat", ((iface == "tun") || (bridge == 0)));

	showhide("client_nat_warn_text", (((nat == 0) || (auth == "secret" && iface == "tun"))));

	showhide("client_local_1", (iface == "tun" && auth == "secret"));
	showhide("client_local_2", (iface == "tap" && (bridge == 0 && auth == "secret")));

	showhide("client_adns", (auth == "tls"));
	showhide("client_reneg", (auth == "tls"));
	showhide("client_gateway_label", (iface == "tap" && rgw > 0));
	showhide("vpn_client_gw", (iface == "tap" && rgw > 0));
	showhide("client_tlsremote", (auth == "tls"));

	showhide("vpn_client_cn", ((auth == "tls") && (tlsremote == 1)));
	showhide("client_cn_label", ((auth == "tls") && (tlsremote == 1)));
	showhide("clientlist_Block", (rgw >= 2));
	showhide("selectiveTable", (rgw >= 2));
	showhide("client_enforce", (rgw >= 2));

	showhide("client_cipher", (ncp != 2));
	showhide("ncp_enable", (auth == "tls"));
	showhide("ncp_ciphers", ((ncp > 0) && (auth == "tls")));
}


function update_rgw_options(){
	currentpolicy = document.form.vpn_client_rgw.value;
	iface = document.form.vpn_client_if_x.value;

	if ((iface == "tap") && (currentpolicy >= 2)) {
		currentpolicy = 1;
		document.form.vpn_client_rgw.value = 1;
	}

	free_options(document.form.vpn_client_rgw);
	add_option(document.form.vpn_client_rgw, "No","0",(currentpolicy == 0));
	add_option(document.form.vpn_client_rgw, "Yes","1",(currentpolicy == 1));
	if (iface == "tun") {
		add_option(document.form.vpn_client_rgw, "Policy Rules","2",(currentpolicy == 2));
		add_option(document.form.vpn_client_rgw, "Policy Rules (strict)","3",(currentpolicy == 3));
	}
}


function edit_Keys(){
	cal_panel_block();
	$("#tlsKey_panel").fadeIn(300);
}
 
function cancel_Keys(){
	this.FromObject ="0";
	$("#tlsKey_panel").fadeOut(300);	

	setTLSTable(openvpn_unit);
}

function save_Keys(){
	switch (openvpn_unit) {
		case "1" :
			document.form.vpn_crt_client1_ca.value = document.getElementById('edit_vpn_crt_client_ca').value;
			document.form.vpn_crt_client1_crt.value = document.getElementById('edit_vpn_crt_client_crt').value;
			document.form.vpn_crt_client1_key.value = document.getElementById('edit_vpn_crt_client_key').value;
			document.form.vpn_crt_client1_static.value = document.getElementById('edit_vpn_crt_client_static').value;
			document.form.vpn_crt_client1_crl.value = document.getElementById('edit_vpn_crt_client_crl').value;
			document.form.vpn_crt_client1_extra.value = document.getElementById('edit_vpn_crt_client_extra').value;
			document.form.vpn_crt_client1_ca.disabled = false;
			document.form.vpn_crt_client1_crt.disabled = false;
			document.form.vpn_crt_client1_key.disabled = false;
			document.form.vpn_crt_client1_static.disabled = false;
			document.form.vpn_crt_client1_crl.disabled = false;
			document.form.vpn_crt_client1_extra.disabled = false;
			break;
		case "2" :
			document.form.vpn_crt_client2_ca.value = document.getElementById('edit_vpn_crt_client_ca').value;
			document.form.vpn_crt_client2_crt.value = document.getElementById('edit_vpn_crt_client_crt').value;
			document.form.vpn_crt_client2_key.value = document.getElementById('edit_vpn_crt_client_key').value;
			document.form.vpn_crt_client2_static.value = document.getElementById('edit_vpn_crt_client_static').value;
			document.form.vpn_crt_client2_crl.value = document.getElementById('edit_vpn_crt_client_crl').value;
			document.form.vpn_crt_client2_extra.value = document.getElementById('edit_vpn_crt_client_extra').value;
			document.form.vpn_crt_client2_ca.disabled = false;
			document.form.vpn_crt_client2_crt.disabled = false;
			document.form.vpn_crt_client2_key.disabled = false;
			document.form.vpn_crt_client2_static.disabled = false;
			document.form.vpn_crt_client2_crl.disabled = false;
			document.form.vpn_crt_client2_extra.disabled = false;
			break;
		case "3" :
			document.form.vpn_crt_client3_ca.value = document.getElementById('edit_vpn_crt_client_ca').value;
			document.form.vpn_crt_client3_crt.value = document.getElementById('edit_vpn_crt_client_crt').value;
			document.form.vpn_crt_client3_key.value = document.getElementById('edit_vpn_crt_client_key').value;
			document.form.vpn_crt_client3_static.value = document.getElementById('edit_vpn_crt_client_static').value;
			document.form.vpn_crt_client3_crl.value = document.getElementById('edit_vpn_crt_client_crl').value;
			document.form.vpn_crt_client3_extra.value = document.getElementById('edit_vpn_crt_client_extra').value;
			document.form.vpn_crt_client3_ca.disabled = false;
			document.form.vpn_crt_client3_crt.disabled = false;
			document.form.vpn_crt_client3_key.disabled = false;
			document.form.vpn_crt_client3_static.disabled = false;
			document.form.vpn_crt_client3_crl.disabled = false;
			document.form.vpn_crt_client3_extra.disabled = false;
			break;
		case "4" :
			document.form.vpn_crt_client4_ca.value = document.getElementById('edit_vpn_crt_client_ca').value;
			document.form.vpn_crt_client4_crt.value = document.getElementById('edit_vpn_crt_client_crt').value;
			document.form.vpn_crt_client4_key.value = document.getElementById('edit_vpn_crt_client_key').value;
			document.form.vpn_crt_client4_static.value = document.getElementById('edit_vpn_crt_client_static').value;
			document.form.vpn_crt_client4_crl.value = document.getElementById('edit_vpn_crt_client_crl').value;
			document.form.vpn_crt_client4_extra.value = document.getElementById('edit_vpn_crt_client_extra').value;
			document.form.vpn_crt_client4_ca.disabled = false;
			document.form.vpn_crt_client4_crt.disabled = false;
			document.form.vpn_crt_client4_key.disabled = false;
			document.form.vpn_crt_client4_static.disabled = false;
			document.form.vpn_crt_client4_crl.disabled = false;
			document.form.vpn_crt_client4_extra.disabled = false;
			break;
		case "5" :
			document.form.vpn_crt_client5_ca.value = document.getElementById('edit_vpn_crt_client_ca').value;
			document.form.vpn_crt_client5_crt.value = document.getElementById('edit_vpn_crt_client_crt').value;
			document.form.vpn_crt_client5_key.value = document.getElementById('edit_vpn_crt_client_key').value;
			document.form.vpn_crt_client5_static.value = document.getElementById('edit_vpn_crt_client_static').value;
			document.form.vpn_crt_client5_crl.value = document.getElementById('edit_vpn_crt_client_crl').value;
			document.form.vpn_crt_client5_extra.value = document.getElementById('edit_vpn_crt_client_extra').value;
			document.form.vpn_crt_client5_ca.disabled = false;
			document.form.vpn_crt_client5_crt.disabled = false;
			document.form.vpn_crt_client5_key.disabled = false;
			document.form.vpn_crt_client5_static.disabled = false;
			document.form.vpn_crt_client5_crl.disabled = false;
			document.form.vpn_crt_client5_extra.disabled = false;
			break;
	}
	
	this.FromObject ="0";
	$("#tlsKey_panel").fadeOut(300);
}

function cal_panel_block(){
	var blockmarginLeft;
	if (window.innerWidth)
		winWidth = window.innerWidth;
	else if ((document.body) && (document.body.clientWidth))
		winWidth = document.body.clientWidth;
		
	if (document.documentElement  && document.documentElement.clientHeight && document.documentElement.clientWidth){
		winWidth = document.documentElement.clientWidth;
	}

	if(winWidth >1050){	
		winPadding = (winWidth-1050)/2;	
		winWidth = 1105;
		blockmarginLeft= (winWidth*0.15)+winPadding;
	}
	else if(winWidth <=1050){
		blockmarginLeft= (winWidth)*0.15+document.body.scrollLeft;	

	}

	document.getElementById("tlsKey_panel").style.marginLeft = blockmarginLeft+"px";
}


function validForm(){
	var addrobj = document.form.vpn_client_addr;
	var alert_str = validator.domainName(addrobj);
	if ((addrobj.value != "") && (alert_str != "") && (!validator.ipv4_addr(addrobj.value))) {
		alert("Invalid remote server address!");
		addrobj.focus();
		return false;
	}

	if (getRadioValue(document.form.vpn_client_userauth) &&
	    (document.form.vpn_client_username.value == "" || document.form.vpn_client_password.value == "")) {
		alert("You must provide a username and a password.")
		if (document.form.vpn_client_username.value == "") {
			document.form.vpn_client_username.focus();
		} else {
			document.form.vpn_client_password.focus();
		}
		return false;
	}

	if (!validator.safeName(document.form.vpn_client_desc) ||
	    !validator.numberRange(document.form.vpn_client_verb, 0, 6) ||
	    !validator.numberRange(document.form.vpn_client_reneg, -1, 32767) ||
	    !validator.numberRange(document.form.vpn_client_connretry, 0, 999) ||
	    !validator.numberRange(document.form.vpn_client_port, 1, 65535))
		return false;

	return true;
}

function applyRule(manual_switch){
	if (!validForm()){
		return false;
	}

	if (manual_switch == 0) {
		showLoading();
		if (client_state != 0) {
			document.form.action_wait.value = 15;
			document.form.action_script.value = "restart_vpnclient"+openvpn_unit;
		}
	}

	tmp_value = "";

	for (var i=1; i < 6; i++) {
		if (i == openvpn_unit) {
			if (getRadioValue(document.form.vpn_client_x_eas) == 1)
				tmp_value += ""+i+",";
		} else {
			if (document.form.vpn_clientx_eas.value.indexOf(''+(i)) >= 0)
				tmp_value += ""+i+","
		}
	}
	document.form.vpn_clientx_eas.value = tmp_value;

	document.form.vpn_client_if.value = document.form.vpn_client_if_x.value;

	var rule_num = document.getElementById('clientlist_table').rows.length;
	var item_num = document.getElementById('clientlist_table').rows[0].cells.length;
	var tmp_value = "";

	for(i=0; i<rule_num; i++){
		tmp_value += "<";
		for(j=0; j<item_num-1; j++){
			var field = document.getElementById('clientlist_table').rows[i].cells[j].innerHTML;
			if (field == "0.0.0.0") field = "";
			tmp_value += field;
			if(j != item_num-2)
				tmp_value += ">";
		}
	}
	if(tmp_value == "<"+"<#IPConnection_VSList_Norule#>" || tmp_value == "<")
		tmp_value = "";

	if (isSupport("hnd")) {
		split_clientlist(tmp_value);
		split_custom2(Base64.encode(document.getElementById("vpn_client_custom_x").value));
	} else {
		document.form.vpn_client_clientlist.value = tmp_value;
		document.form.vpn_client_cust2.value = Base64.encode(document.getElementById("vpn_client_custom_x").value);
	}

	if (((enforce_ori != getRadioValue(document.form.vpn_client_enforce)) ||
	     (policy_ori != document.form.vpn_client_rgw.value)) &&
	    (client_state == 0) && (manual_switch == 0))
		document.form.action_script.value += "start_vpnrouting"+openvpn_unit;

	if ((dnsmode_ori != document.form.vpn_client_adns.value) && (client_state != 0))
		document.form.action_script.value += ";restart_dnsmasq"

	document.form.submit();
}

function split_clientlist(clientlist){
	var counter = 0;
	document.form.vpn_client_clientlist.value = clientlist.substring(counter, (counter+=255));

	document.form.vpn_client_clientlist1.value = clientlist.substring(counter, (counter+=255));
	document.form.vpn_client_clientlist2.value = clientlist.substring(counter, (counter+=255));
	document.form.vpn_client_clientlist3.value = clientlist.substring(counter, (counter+=255));
	document.form.vpn_client_clientlist4.value = clientlist.substring(counter, (counter+=255));
	document.form.vpn_client_clientlist5.value = clientlist.substring(counter, (counter+=255));
}

function split_custom2(cust2){
	var counter = 0;
	document.form.vpn_client_cust2.value = cust2.substring(counter, (counter+=255));

	document.form.vpn_client_cust21.value = cust2.substring(counter, (counter+=255));
	document.form.vpn_client_cust22.value = cust2.substring(counter, (counter+=255));
}

function change_vpn_unit(val){
	document.form.action_mode.value = "change_vpn_client_unit";
	document.form.action = "apply.cgi";
	document.form.target = "";
	document.form.submit();
}

/* password item show or not */
function pass_checked(obj){
	switchType(obj, document.form.show_pass_1.checked, true);
}

function ImportOvpn(){
	if (document.getElementById('ovpnfile').value == "") return false;

	document.getElementById('importOvpnFile').style.display = "none";
	document.getElementById('loadingicon').style.display = "";

	document.form.action = "vpnupload.cgi";
	document.form.enctype = "multipart/form-data";
	document.form.encoding = "multipart/form-data";
	document.form.vpn_upload_unit.value = openvpn_unit;

	document.form.submit();
	setTimeout("ovpnFileChecker();",2000);
}

var vpn_upload_state = "init";
function ovpnFileChecker(){
	var missing;

	document.getElementById("importOvpnFile").innerHTML = "<#Main_alert_proceeding_desc3#>";

	$.ajax({
			url: '/ajax_openvpn_server.asp',
			dataType: 'script',
			timeout: 1500,
			error: function(xhr){
				setTimeout("ovpnFileChecker();",1000);
			},

			success: function(){
				document.getElementById('importOvpnFile').style.display = "";
				document.getElementById('loadingicon').style.display = "none";

				if(vpn_upload_state == "init"){
					setTimeout("ovpnFileChecker();",1000);
				}
				else if(vpn_upload_state > 0){
					document.getElementById("importOvpnFile").innerHTML = "Warning!";
					missing = upload_build_report(vpn_upload_state);
					alert("Warning (" + vpn_upload_state +") while importing file - you will need to manually provide the " + missing.replace(/,\s*$/, "") + " content, on the keys/certificates page.");
					setTimeout("location.href='Advanced_OpenVPNClient_Content.asp';", 3000);
				}
				else{
					setTimeout("location.href='Advanced_OpenVPNClient_Content.asp';", 3000);
				}
			}
	});
}

function upload_build_report(ret){

	var missing = "";

	if (ret & 1)
		missing += "CA, ";
	if (ret & 2)
		missing += "Certificate, ";
	if (ret & 4)
		missing += "Key, ";
	if (ret & 8)
		missing += "Static Key, ";
	if (ret & 16)
		missing += "CRL, ";
	if (ret & 32)
		missing += "Extra certificate, "
	return missing;
}

function update_local_ip(object){

	if (object.name == "vpn_client_local_1")
		document.form.vpn_client_local_2.value = object.value;
	else if (object.name == "vpn_client_local_2")
		document.form.vpn_client_local_1.value = object.value;

	document.form.vpn_client_local.value = object.value;
}

function showclientlist(){
	var clientlist_row = clientlist_array.split('&#60');
	var code = "";
	var width = ["24%", "29%", "25%", "10%", "12%"];

	code +='<table width="100%" cellspacing="0" cellpadding="4" align="center" class="list_table" id="clientlist_table">';
	if(clientlist_row.length == 1)
		code +='<tr><td style="color:#FFCC00;" colspan="6"><#IPConnection_VSList_Norule#></td></tr>';
	else{
		for(var i = 1; i < clientlist_row.length; i++){
			code +='<tr id="row'+i+'">';
			var clientlist_col = clientlist_row[i].split('&#62');
				for(var j = 0; j < clientlist_col.length; j++){
					if ((j == 1 || j == 2) && clientlist_col[j] == "0.0.0.0")
						clientlist_col[j] = "";
					code +='<td width="' + width[j] +'">'+ clientlist_col[j] +'</td>';
				}
				if (j < 4) {
					code +='<td width="' + width[j++] +'">VPN</td>';
				}
				code +='<td width="' + width[j] +'">';
				code +='<input class="remove_btn" onclick="del_Row(this);" value=""/></td></tr>';
		}
	}

  code +='</table>';
	document.getElementById("clientlist_Block").innerHTML = code;
}

function addRow(obj, head){
	if(head == 1)
		clientlist_array += "&#60" /*&#60*/
	else
		clientlist_array += "&#62" /*&#62*/

	clientlist_array += obj.value;
	obj.value = "";
}

function addRow_Group(upper){
	var rule_num = document.getElementById('clientlist_table').rows.length;
	var item_num = document.getElementById('clientlist_table').rows[0].cells.length;
	if(rule_num >= upper){
		alert("<#JS_itemlimit1#> " + upper + " <#JS_itemlimit2#>");
		return false;
	}

	if (!validator.safeName(document.form.clientlist_deviceName))
		return false;

	if (!validator.ipv4cidr(document.form.clientlist_ipAddr)) {
		document.form.clientlist_ipAddr.focus();
		document.form.clientlist_ipAddr.select();
		return false;
	}

	if (!validator.ipv4cidr(document.form.clientlist_dstipAddr)) {
		document.form.clientlist_dstipAddr.focus();
		document.form.clientlist_dstipAddr.select();
		return false;
	}

	if(item_num >=2){
		for(i=0; i<rule_num; i++){
				if(document.form.clientlist_ipAddr.value.toLowerCase() == document.getElementById('clientlist_table').rows[i].cells[1].innerHTML.toLowerCase() &&
				   document.form.clientlist_dstipAddr.value.toLowerCase() == document.getElementById('clientlist_table').rows[i].cells[2].innerHTML.toLowerCase()){
					alert("<#JS_duplicate#>");
					document.form.clientlist_ipAddr.focus();
					document.form.clientlist_ipAddr.select();
					return false;
				}
		}
	}

	addRow(document.form.clientlist_deviceName ,1);
	addRow(document.form.clientlist_ipAddr, 0);
	addRow(document.form.clientlist_dstipAddr, 0);
	addRow(document.form.clientlist_iface, 0);
	document.form.clientlist_iface.value = "VPN";
	showclientlist();
}

function del_Row(r){
	var i=r.parentNode.parentNode.rowIndex;
	document.getElementById('clientlist_table').deleteRow(i);

	var clientlist_value = "";
	for(k=0; k<document.getElementById('clientlist_table').rows.length; k++){
		clientlist_value += "&#60";
		clientlist_value += document.getElementById('clientlist_table').rows[k].cells[0].innerHTML;
		clientlist_value += "&#62";
		clientlist_value += document.getElementById('clientlist_table').rows[k].cells[1].innerHTML;
		clientlist_value += "&#62";
		clientlist_value += document.getElementById('clientlist_table').rows[k].cells[2].innerHTML;
		clientlist_value += "&#62";
		clientlist_value += document.getElementById('clientlist_table').rows[k].cells[3].innerHTML;
	}

	clientlist_array = clientlist_value;
	if(clientlist_array == "")
		showclientlist();
}

function hideClients_Block(evt){
	if(typeof(evt) != "undefined"){
		if(!evt.srcElement)
			evt.srcElement = evt.target; // for Firefox

		if(evt.srcElement.id == "pull_arrow" || evt.srcElement.id == "ClientList_Block"){
			return;
		}
	}

	document.getElementById("pull_arrow").src = "/images/arrow-down.gif";
	document.getElementById('ClientList_Block_PC').style.display='none';
}

function pullLANIPList(obj){
	var element = document.getElementById('ClientList_Block_PC');
	var isMenuopen = element.offsetWidth > 0 || element.offsetHeight > 0;
	if(isMenuopen == 0){
		obj.src = "/images/arrow-top.gif"
		element.style.display = 'block';
	}
	else
		hideClients_Block();
}

function setClientIP(name, ipaddr){
	document.form.clientlist_ipAddr.value = ipaddr;
	document.form.clientlist_deviceName.value = name;
	hideClients_Block();
}

function getConnStatus() {
	$.ajax({
		url: 'ajax_vpn_status.asp',
		dataType: 'script',
		error: function(xhr){
			getConnStatus();
		},
		success: function(response){
			showConnStatus();
		}
	});
}

function showConnStatus() {
	switch (openvpn_unit) {
		case "1":
			client_state = vpnc_state_t1;
			client_errno = vpnc_errno_t1;
			localip = vpn_client1_ip;
			remoteip = vpn_client1_rip;
			break;
		case "2":
			client_state = vpnc_state_t2;
			client_errno = vpnc_errno_t2;
			localip = vpn_client2_ip;
			remoteip = vpn_client2_rip;
			break;
		case "3":
			client_state = vpnc_state_t3;
			client_errno = vpnc_errno_t3;
			localip = vpn_client3_ip;
			remoteip = vpn_client3_rip;
			break;
		case "4":
			client_state = vpnc_state_t4;
			client_errno = vpnc_errno_t4;
			localip = vpn_client4_ip;
			remoteip = vpn_client4_rip;
			break;
		case "5":
			client_state = vpnc_state_t5;
			client_errno = vpnc_errno_t5;
			localip = vpn_client5_ip;
			remoteip = vpn_client5_rip;
			break;
	}

	switch (client_state) {
		case "1":	// Connecting
			code = "Connecting...";
			setTimeout("getConnStatus()",2000);
			break;
		case "2":	// COnnected
			code = "Connected (Local: "+ localip + " - Public: " + remoteip + ") <a href='#' style='padding-left:12px;text-decoration:underline;' onclick='refreshVPNIP();'>Refresh</a>";
			break;
		case "-1":
			switch (client_errno) {
				case "1":
					code = "Error - IP conflict!";
					break;
				case "2":
					code = "Error - Routing conflict!";
					break;
				case "4":
					code = "Error - SSL/TLS issue!";
				break;
				case "5":
					code = "Error - DH issue!";
					break;
				case "6":
					code = "Error - Authentication failure!";
					break;
				default:
					code = "Error - check configuration!";
					break;
			}
			setTimeout("getConnStatus()",2000);
		break;
		default:
			code = "";
			break;
	}
	document.getElementById("vpn_state_msg").innerHTML = "<span>" + code + "</span>";
}

function defaultSettings() {
	if (confirm("WARNING: This will reset this OpenVPN client to factory default settings!\n\nKeys and certificates associated to this instance will also be DELETED!\n\nProceed?")) {
		document.form.action_script.value = "stop_vpnclient" + openvpn_unit + ";clearvpnclient" + openvpn_unit;
		document.form.action_wait.value = 15;
		showLoading();
		document.form.submit();
	} else {
		return false;
	}
}

function refreshVPNIP() {
	httpApi.nvramSet({"action_mode": "refresh_vpn_ip"}, function(){setTimeout("getConnStatus()", 2000);});
}

</script>
</head>

<body onload="initial();" onunLoad="return unload_body();">
	<div id="tlsKey_panel"  class="contentM_qis" style="box-shadow: 3px 3px 10px #000;">
		<table class="QISform_wireless" border=0 align="center" cellpadding="5" cellspacing="0">
			<tr>
				<div class="description_down">Keys and Certificates</div>
			</tr>
			<tr>
				<div style="margin-left:30px; margin-top:10px;">
					<p><#vpn_openvpn_KC_Edit1#> <span style="color:#FFCC00;">----- BEGIN xxx ----- </span>/<span style="color:#FFCC00;"> ----- END xxx -----</span> <#vpn_openvpn_KC_Edit2#>
					<p>Limit: 7999 characters per field
				</div>
				<div style="margin:5px;*margin-left:-5px;width: 730px; height: 2px;" class="splitLine"></div>
			</tr>			
			<!--===================================Beginning of tls Content===========================================-->

			<tr>
				<td valign="top">
					<table width="700px" border="0" cellpadding="4" cellspacing="0">
						<tbody>
							<tr>
								<td valign="top">
									<table width="100%" id="page1_tls" border="1" align="center" cellpadding="4" cellspacing="0" class="FormTable">
										<tr>
											<th>Static Key</th>
											<td>
												<textarea rows="8" class="textarea_ssh_table" spellcheck="false" id="edit_vpn_crt_client_static" name="edit_vpn_crt_client_static" cols="65" maxlength="7999"></textarea>
											</td>
										</tr>
										<tr>
											<th id="manualCa">Certificate Authority</th>
											<td>
												<textarea rows="8" class="textarea_ssh_table" spellcheck="false" id="edit_vpn_crt_client_ca" name="edit_vpn_crt_client_ca" cols="65" maxlength="7999"></textarea>
											</td>
										</tr>
										<tr>
											<th id="manualCert">Client Certificate</th>
											<td>
												<textarea rows="8" class="textarea_ssh_table" spellcheck="false" id="edit_vpn_crt_client_crt" name="edit_vpn_crt_client_crt" cols="65" maxlength="7999"></textarea>
											</td>
										</tr>
										<tr>
											<th id="manualKey">Client Key</th>
											<td>
												<textarea rows="8" class="textarea_ssh_table" spellcheck="false" id="edit_vpn_crt_client_key" name="edit_vpn_crt_client_key" cols="65" maxlength="7999"></textarea>
											</td>
										</tr>
										<tr>
											<th id="manualKey">Certificate Revocation List<br><br><i>(Optional)</i></th>
											<td>
												<textarea rows="8" class="textarea_ssh_table" spellcheck="false" id="edit_vpn_crt_client_crl" name="edit_vpn_crt_client_crl" cols="65" maxlength="7999"></textarea>
											</td>
										</tr>
										<tr>
											<th id="manualKey">Extra Chain Certificates<br><br><i>(Optional)</i></th>
											<td>
												<textarea rows="8" class="textarea_ssh_table" spellcheck="false" id="edit_vpn_crt_client_extra" name="edit_vpn_crt_client_extra" cols="65" maxlength="7999"></textarea>
											</td>
										</tr>
									</table>
								</td>
							</tr>						
						</tbody>						
					</table>
					<div style="margin-top:5px;width:100%;text-align:center;">
						<input class="button_gen" type="button" onclick="cancel_Keys();" value="<#CTL_Cancel#>">
						<input class="button_gen" type="button" onclick="save_Keys();" value="<#CTL_onlysave#>">
					</div>
				</td>
			</tr>
      <!--===================================Ending of tls Content===========================================-->			

		</table>		
	</div>

<div id="TopBanner"></div>

<div id="Loading" class="popup_bg"></div>

<iframe name="hidden_frame" id="hidden_frame" src="" width="0" height="0" frameborder="0"></iframe>

<form method="post" name="form" id="ruleForm" action="/start_apply.htm" target="hidden_frame">
<input type="hidden" name="current_page" value="Advanced_OpenVPNClient_Content.asp">
<input type="hidden" name="next_page" value="Advanced_OpenVPNClient_Content.asp">
<input type="hidden" name="modified" value="0">
<input type="hidden" name="action_mode" value="apply">
<input type="hidden" name="action_script" value="">
<input type="hidden" name="action_wait" value="5">
<input type="hidden" name="first_time" value="">
<input type="hidden" name="SystemCmd" value="">
<input type="hidden" name="preferred_lang" id="preferred_lang" value="<% nvram_get("preferred_lang"); %>">
<input type="hidden" name="firmver" value="<% nvram_get("firmver"); %>">
<input type="hidden" name="vpn_clientx_eas" value="<% nvram_clean_get("vpn_clientx_eas"); %>">
<input type="hidden" name="vpn_crt_client1_ca" value="" disabled>
<input type="hidden" name="vpn_crt_client1_crt" value="" disabled>
<input type="hidden" name="vpn_crt_client1_key" value="" disabled>
<input type="hidden" name="vpn_crt_client1_static" value="" disabled>
<input type="hidden" name="vpn_crt_client1_crl" value="" disabled>
<input type="hidden" name="vpn_crt_client1_extra" value="" disabled>
<input type="hidden" name="vpn_crt_client2_ca" value="" disabled>
<input type="hidden" name="vpn_crt_client2_crt" value="" disabled>
<input type="hidden" name="vpn_crt_client2_key" value="" disabled>
<input type="hidden" name="vpn_crt_client2_static" value="" disabled>
<input type="hidden" name="vpn_crt_client2_crl" value="" disabled>
<input type="hidden" name="vpn_crt_client2_extra" value="" disabled>
<input type="hidden" name="vpn_crt_client3_ca" value="" disabled>
<input type="hidden" name="vpn_crt_client3_crt" value="" disabled>
<input type="hidden" name="vpn_crt_client3_key" value="" disabled>
<input type="hidden" name="vpn_crt_client3_static" value="" disabled>
<input type="hidden" name="vpn_crt_client3_crl" value="" disabled>
<input type="hidden" name="vpn_crt_client3_extra" value="" disabled>
<input type="hidden" name="vpn_crt_client4_ca" value="" disabled>
<input type="hidden" name="vpn_crt_client4_crt" value="" disabled>
<input type="hidden" name="vpn_crt_client4_key" value="" disabled>
<input type="hidden" name="vpn_crt_client4_static" value="" disabled>
<input type="hidden" name="vpn_crt_client4_crl" value="" disabled>
<input type="hidden" name="vpn_crt_client4_extra" value="" disabled>
<input type="hidden" name="vpn_crt_client5_ca" value="" disabled>
<input type="hidden" name="vpn_crt_client5_crt" value="" disabled>
<input type="hidden" name="vpn_crt_client5_key" value="" disabled>
<input type="hidden" name="vpn_crt_client5_static" value="" disabled>
<input type="hidden" name="vpn_crt_client5_crl" value="" disabled>
<input type="hidden" name="vpn_crt_client5_extra" value="" disabled>
<input type="hidden" name="vpn_upload_type" value="ovpn">
<input type="hidden" name="vpn_upload_unit" value="<% nvram_get("vpn_client_unit"); %>">
<input type="hidden" name="vpn_client_if" value="<% nvram_get("vpn_client_if"); %>">
<input type="hidden" name="vpn_client_local" value="<% nvram_get("vpn_client_local"); %>">
<input type="hidden" name="vpn_client_clientlist" value="<% nvram_clean_get("vpn_client_clientlist"); %>">
<input type="hidden" name="vpn_client_clientlist1" value="<% nvram_clean_get("vpn_client_clientlist1"); %>">
<input type="hidden" name="vpn_client_clientlist2" value="<% nvram_clean_get("vpn_client_clientlist2"); %>">
<input type="hidden" name="vpn_client_clientlist3" value="<% nvram_clean_get("vpn_client_clientlist3"); %>">
<input type="hidden" name="vpn_client_clientlist4" value="<% nvram_clean_get("vpn_client_clientlist4"); %>">
<input type="hidden" name="vpn_client_clientlist5" value="<% nvram_clean_get("vpn_client_clientlist5"); %>">
<input type="hidden" name="vpn_client_cust2" value="<% nvram_get("vpn_client_cust2"); %>">
<input type="hidden" name="vpn_client_cust21" value="<% nvram_get("vpn_client_cust21"); %>">
<input type="hidden" name="vpn_client_cust22" value="<% nvram_get("vpn_client_cust22"); %>">

<table class="content" align="center" cellpadding="0" cellspacing="0">
  <tr>
    <td width="17">&nbsp;</td>
    <td valign="top" width="202">
      <div id="mainMenu"></div>
      <div id="subMenu"></div>
    </td>
    <td valign="top">
        <div id="tabMenu" class="submenuBlock"></div>

      <!--===================================Beginning of Main Content===========================================-->
      <table width="98%" border="0" align="left" cellpadding="0" cellspacing="0">
        <tr>
          <td valign="top">
            <table width="760px" border="0" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3"  class="FormTitle" id="FormTitle">
                <tbody>
                <tr bgcolor="#4D595D">
                <td valign="top">
                <div>&nbsp;</div>
                <div class="formfonttitle">OpenVPN Client Settings</div>
		<div id="divSwitchMenu" style="margin-top:-40px;float:right;"></div>
		<div style="margin:10px 0 10px 5px;" class="splitLine"></div>
		<div class="formfontdesc">
                        <p>Before starting the service make sure you properly configure it, including
                           the required keys,<br>otherwise you will be unable to turn it on.
                        <p><br>In case of problem, see the <a style="font-weight: bolder;text-decoration:underline;" class="hyperlink" href="Main_LogStatus_Content.asp">System Log</a> for any error message related to openvpn.
                </div>

				<table width="100%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3"  class="FormTable">
					<thead>
						<tr>
							<td colspan="2">Client control</td>
						</tr>
					</thead>
					<tr id="client_unit">
						<th>Select client instance</th>
						<td>
							<select name="vpn_client_unit" class="input_option" onChange="change_vpn_unit(this.value);">
							</select>
						</td>
					</tr>
					<tr id="service_enable_button">
						<th>Service state</th>
						<td>
							<div class="left" style="width:94px; float:left; cursor:pointer;" id="radio_service_enable"></div>
							<script type="text/javascript">

								$('#radio_service_enable').iphoneSwitch((client_state > 0),
									 function() {
										document.form.action_script.value = "start_vpnclient" + openvpn_unit;
										document.form.action_wait.value = 10;
										parent.showLoading();
										applyRule(1);
										return true;
									 },
									 function() {
										document.form.action_script.value = "stop_vpnclient" + openvpn_unit;
										document.form.action_wait.value = 10;
										parent.showLoading();
										applyRule(1)
										return true;
									 },
									 {
										switch_on_container_path: '/switcherplugin/iphone_switch_container_off.png'
									 }
								);
							</script>
							<div style="height:30px;line-height:30px;" id="vpn_state_msg"></div>
					    </td>
					</tr>
					<tr>
						<th>Automatic start at boot time</th>
						<td>
							<input type="radio" name="vpn_client_x_eas" class="input" value="1"><#checkbox_Yes#>
							<input type="radio" name="vpn_client_x_eas" class="input" value="0"><#checkbox_No#>
						</td>
					</tr>
					<tr>
						<th>Description</th>
						<td>
							<input type="text" maxlength="25" class="input_25_table" name="vpn_client_desc" value="<% nvram_get("vpn_client_desc"); %>">
						</td>
					</tr>
					<tr>
						<th><#vpn_openvpnc_importovpn#></th>
						<td>
							<input id="ovpnfile" type="file" name="file" class="input" style="color:#FFCC00;*color:#000;">
							<input id="" class="button_gen" onclick="ImportOvpn();" type="button" value="<#CTL_upload#>" />
								<img id="loadingicon" style="margin-left:5px;display:none;" src="/images/InternetScan.gif">
								<span id="importOvpnFile" style="display:none;"><#Main_alert_proceeding_desc3#></span>
						</td>
					</tr>
				</table>

				<table width="100%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3"  class="FormTable">
					<thead>
						<tr>
							<td colspan="2">Network Settings</td>
						</tr>
					</thead>
					<tr>
						<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(32,4);"><#vpn_openvpn_interface#></a></th>
						<td>
							<select name="vpn_client_if_x"  onclick="update_rgw_options();update_visibility();" class="input_option">
							</select>
						</td>
					</tr>
					<tr>
						<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(32,5);"><#IPConnection_VServerProto_itemname#></a></th>
						<td>
							<select name="vpn_client_proto" class="input_option">
								<option value="tcp-client" <% nvram_match("vpn_client_proto","tcp-client","selected"); %> >TCP</option>
								<option value="udp" <% nvram_match("vpn_client_proto","udp","selected"); %> >UDP</option>
							</select>
						</td>
					</tr>
					<tr>
						<th>Server Address and Port</th>
						<td>
							<label>Address:</label><input type="text" maxlength="128" class="input_25_table" name="vpn_client_addr" value="<% nvram_get("vpn_client_addr"); %>" autocorrect="off" autocapitalize="off" spellcheck="false">
							<label style="margin-left: 4em;">Port:</label><input type="text" maxlength="5" class="input_6_table" name="vpn_client_port" onKeyPress="return validator.isNumber(this,event);" value="<% nvram_get("vpn_client_port"); %>" >
						</td>
					</tr>
					<tr id="client_adns">
						<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(50,24);">Accept DNS Configuration</a></th>
						<td>
							<select name="vpn_client_adns" class="input_option">
								<option value="0" <% nvram_match("vpn_client_adns","0","selected"); %> >Disabled</option>
								<option value="1" <% nvram_match("vpn_client_adns","1","selected"); %> >Relaxed</option>
								<option value="2" <% nvram_match("vpn_client_adns","2","selected"); %> >Strict</option>
								<option value="3" <% nvram_match("vpn_client_adns","3","selected"); %> >Exclusive</option>
							</select>
						</td>
					</tr>
					<tr id="client_bridge">
						<th>Server is on the same subnet</th>
						<td>
							<input type="radio" name="vpn_client_bridge" class="input" value="1" onclick="update_visibility();" <% nvram_match_x("", "vpn_client_bridge", "1", "checked"); %>><#checkbox_Yes#>
							<input type="radio" name="vpn_client_bridge" class="input" value="0" onclick="update_visibility();" <% nvram_match_x("", "vpn_client_bridge", "0", "checked"); %>><#checkbox_No#>
							<span id="client_bridge_warn_text">Warning: Cannot bridge distinct subnets. Will default to routed mode.</span>
						</td>
					</tr>
					<tr id="client_nat">
						<th>Create NAT on tunnel</th>
						<td>
							<input type="radio" name="vpn_client_nat" class="input" value="1" onclick="update_visibility();" <% nvram_match_x("", "vpn_client_nat", "1", "checked"); %>><#checkbox_Yes#>
							<input type="radio" name="vpn_client_nat" class="input" value="0" onclick="update_visibility();" <% nvram_match_x("", "vpn_client_nat", "0", "checked"); %>><#checkbox_No#>
							<span id="client_nat_warn_text">Routes must be configured manually.</span>
						</td>
					</tr>
					<tr>
						<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(50,30);">Inbound Firewall</a></th>
						<td>
							<input type="radio" name="vpn_client_fw" class="input" value="1" <% nvram_match_x("", "vpn_client_fw", "1", "checked"); %>>Block
							<input type="radio" name="vpn_client_fw" class="input" value="0" <% nvram_match_x("", "vpn_client_fw", "0", "checked"); %>>Allow
						</td>
					</tr>
					<tr id="client_local_1">
						<th>Local/remote endpoint addresses</th>
						<td>
							<input type="text" maxlength="15" class="input_15_table" name="vpn_client_local_1" onkeypress="return validator.isIPAddr(this, event);" onblur="update_local_ip(this);" value="<% nvram_get("vpn_client_local"); %>">
							<input type="text" maxlength="15" class="input_15_table" name="vpn_client_remote" onkeypress="return validator.isIPAddr(this, event);" value="<% nvram_get("vpn_client_remote"); %>">
						</td>
					</tr>
					<tr id="client_local_2">
						<th>Tunnel address/netmask</th>
						<td>
							<input type="text" maxlength="15" class="input_15_table" name="vpn_client_local_2" onkeypress="return validator.isIPAddr(this, event);" onblur="update_local_ip(this);" value="<% nvram_get("vpn_client_local"); %>">
							<input type="text" maxlength="15" class="input_15_table" name="vpn_client_nm" onkeypress="return validator.isIPAddr(this, event);" value="<% nvram_get("vpn_client_nm"); %>">
						</td>
					</tr>
				</table>

				<table width="100%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3"  class="FormTable">
					<thead>
						<tr>
							<td colspan="2">Authentication Settings</td>
						</tr>
					</thead>
					<tr>
						<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(32,7);"><#vpn_openvpn_Auth#></a></th>
						<td>
							<select name="vpn_client_crypt" class="input_option" onclick="update_visibility();">
								<option value="tls" <% nvram_match("vpn_client_crypt","tls","selected"); %> >TLS</option>
								<option value="secret" <% nvram_match("vpn_client_crypt","secret","selected"); %> >Static Key</option>
							</select>
						</td>
					</tr>
					<tr id="client_userauth">
						<th>Username/Password Authentication</th>
						<td>
							<input type="radio" name="vpn_client_userauth" class="input" value="1" onclick="update_visibility();" <% nvram_match_x("", "vpn_client_userauth", "1", "checked"); %>><#checkbox_Yes#>
							<input type="radio" name="vpn_client_userauth" class="input" value="0" onclick="update_visibility();" <% nvram_match_x("", "vpn_client_userauth", "0", "checked"); %>><#checkbox_No#>
						</td>
					</tr>
					<tr id="client_username">
						<th>Username</th>
						<td>
							<input type="text" maxlength="255" class="input_25_table" name="vpn_client_username" value="<% nvram_clean_get("vpn_client_username"); %>" autocorrect="off" autocapitalize="off" spellcheck="false">
						</td>
					</tr>
					<tr id="client_password">
						<th>Password</th>
						<td>
							<input type="password" autocomplete="new-password" maxlength="255" class="input_25_table" name="vpn_client_password" value="<% nvram_clean_get("vpn_client_password"); %>">
							<input type="checkbox" name="show_pass_1" onclick="pass_checked(document.form.vpn_client_password)"><#QIS_show_pass#>
						</td>
					</tr>
					<tr id="client_useronly">
						<th><#vpn_openvpn_AuthOnly#></th>
						<td>
							<input type="radio" name="vpn_client_useronly" class="input" value="1" onclick="update_visibility();" <% nvram_match_x("", "vpn_client_useronly", "1", "checked"); %>><#checkbox_Yes#>
							<input type="radio" name="vpn_client_useronly" class="input" value="0" onclick="update_visibility();" <% nvram_match_x("", "vpn_client_useronly", "0", "checked"); %>><#checkbox_No#>
							<span id="client_ca_warn_text">Warning: You must define a Certificate Authority.</span>
						</td>
					</tr>
				</table>

				<table width="100%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3"  class="FormTable">
					<thead>
						<tr>
							<td colspan="2">Crypto Settings</td>
						</tr>
					</thead>
					<tr>
						<th>Keys and Certificates</th>
						<td>
							<input type="button" onclick="edit_Keys();" value="Edit..."></td>
						</td>
					</tr>
					<tr id="ncp_enable">
						<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(50,16);">Cipher Negotiation</a></th>
						<td>
							<select name="vpn_client_ncp_enable" onclick="update_visibility();" class="input_option">
								<option value="0" <% nvram_match("vpn_client_ncp_enable","0","selected"); %> >Disabled</option>
								<option value="1" <% nvram_match("vpn_client_ncp_enable","1","selected"); %> >Enable (with fallback)</option>
								<option value="2" <% nvram_match("vpn_client_ncp_enable","2","selected"); %> >Enable</option>
							</select>
						</td>
					</tr>
					<tr id="ncp_ciphers">
						<th>Negotiable ciphers</th>
						<td>
							<input type="text" maxlength="255" class="input_32_table" name="vpn_client_ncp_ciphers" value="<% nvram_get("vpn_client_ncp_ciphers"); %>" autocorrect="off" autocapitalize="off" spellcheck="false">
						</td>
					</tr>
					<tr id="client_cipher">
						<th>Legacy/fallback cipher</th>
						<td>
							<select name="vpn_client_cipher" class="input_option">
								<option value="<% nvram_get("vpn_client_cipher"); %>" selected><% nvram_get("vpn_client_cipher"); %></option>
							</select>
						</td>
					</tr>
					<tr id="client_hmac">
						<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(32,10);">TLS control channel security<br><i>(tls-auth / tls-crypt)</i></a></th>
						<td>
							<select name="vpn_client_hmac" class="input_option">
								<option value="-1" <% nvram_match("vpn_client_hmac","-1","selected"); %> >Disabled</option>
								<option value="2" <% nvram_match("vpn_client_hmac","2","selected"); %> >Bi-directional Auth</option>
								<option value="0" <% nvram_match("vpn_client_hmac","0","selected"); %> >Incoming Auth (0)</option>
								<option value="1" <% nvram_match("vpn_client_hmac","1","selected"); %> >Outgoing Auth (1)</option>
								<option value="3" <% nvram_match("vpn_client_hmac","3","selected"); %> >Encrypt Channel</option>
							</select>
						</td>
					</tr>
					<tr>
						<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(32,26);">Auth digest</a></th>
						<td>
							<select name="vpn_client_digest" class="input_option">
								<option value="<% nvram_get("vpn_client_digest"); %>" selected><% nvram_get("vpn_client_digest"); %></option>
							</select>
						</td>
					</tr>
				</table>

				<table width="100%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3"  class="FormTable">
					<thead>
						<tr>
							<td colspan="2">Advanced Settings</td>
						</tr>
					</thead>
					<tr>
						<th>Log verbosity<br><i>(0-6, default=3)</i></th>
						<td>
							<input type="text" maxlength="2" class="input_6_table" name="vpn_client_verb" onKeyPress="return validator.isNumber(this,event);" value="<% nvram_get("vpn_client_verb"); %>">
						</td>
					</tr>
					<tr>
						<th><#vpn_openvpn_Compression#></th>
						<td>
							<select name="vpn_client_comp" class="input_option">
								<option value="-1" <% nvram_match("vpn_client_comp","-1","selected"); %> >Disabled</option>
								<option value="no" <% nvram_match("vpn_client_comp","no","selected"); %> >None</option>
								<option value="yes" <% nvram_match("vpn_client_comp","yes","selected"); %> >LZO</option>
								<option value="adaptive" <% nvram_match("vpn_client_comp","adaptive","selected"); %> > LZO Adaptive</option>
								<option value="lz4" <% nvram_match("vpn_client_comp","lz4","selected"); %> >LZ4</option>
								<option value="lz4-v2" <% nvram_match("vpn_client_comp","lz4-v2","selected"); %> >LZ4-V2</option>
							</select>
						</td>
					</tr>
					<tr id="client_reneg">
						<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(32,19);">TLS Renegotiation Time<br><i>(in seconds, -1 for default)</a></th>
						<td>
							<input type="text" maxlength="5" class="input_6_table" name="vpn_client_reneg" value="<% nvram_get("vpn_client_reneg"); %>">
						</td>
					</tr>
					<tr>
						<th>Connection Retry attempts<br><i>(0 for infinite)</th>
						<td>
							<input type="text" maxlength="3" class="input_6_table" name="vpn_client_connretry" value="<% nvram_get("vpn_client_connretry"); %>">
						</td>
					</tr>
					<tr id="client_tlsremote">
						<th>Verify Server Certificate</th>
						<td>
							<input type="radio" name="vpn_client_tlsremote" class="input" onclick="update_visibility();" value="1" <% nvram_match_x("", "vpn_client_tlsremote", "1", "checked"); %>><#checkbox_Yes#>
							<input type="radio" name="vpn_client_tlsremote" class="input" onclick="update_visibility();" value="0" <% nvram_match_x("", "vpn_client_tlsremote", "0", "checked"); %>><#checkbox_No#>
							<label style="padding-left:3em;" id="client_cn_label">Common name:</label><input type="text" maxlength="255" class="input_25_table" id="vpn_client_cn" name="vpn_client_cn" value="<% nvram_get("vpn_client_cn"); %>">
						</td>
					</tr>
					<tr>
						<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(50,19);">Force Internet traffic through tunnel</a></th>
						<td colspan="2">
							<select name="vpn_client_rgw" class="input_option" onChange="update_visibility();">
							</select>
							<label style="padding-left:3em;" id="client_gateway_label">Gateway:</label><input type="text" maxlength="15" class="input_15_table" id="vpn_client_gw" name="vpn_client_gw" onkeypress="return validator.isIPAddr(this, event);" value="<% nvram_get("vpn_client_gw"); %>">
						</td>
					</tr>
					<tr id="client_enforce">
						<th>Block routed clients if tunnel goes down</th>
						<td>
							<input type="radio" name="vpn_client_enforce" class="input" value="1" <% nvram_match_x("", "vpn_client_enforce", "1", "checked"); %>><#checkbox_Yes#>
							<input type="radio" name="vpn_client_enforce" class="input" value="0" <% nvram_match_x("", "vpn_client_enforce", "0", "checked"); %>><#checkbox_No#>
						</td>
					</tr>
				</table>

				<table id="selectiveTable" width="100%" border="1" align="center" cellpadding="4" cellspacing="0" class="FormTable_table" style="margin-top:8px;">
					<thead>
						<tr>
							<td colspan="5">Rules for routing client traffic through the tunnel (<#List_limit#>&nbsp;100)</td>
						</tr>
					</thead>
					<tr>
						<th><#IPConnection_autofwDesc_itemname#></th>
						<th>Source IP</th>
						<th>Destination IP</th>
						<th>Iface</th>
						<th><#list_add_delete#></th>
					</tr>
					<tr>
						<td width="24%">
							<input type="text" class="input_15_table" maxlength="15" name="clientlist_deviceName" onClick="hideClients_Block();" onKeyPress="return validator.isString(this, event);">
						</td>
						<td width="29%">
							<input type="text" class="input_18_table" maxlength="18" name="clientlist_ipAddr" onKeyPress="return validator.isIPAddrPlusNetmask(this, event)" onClick="hideClients_Block();" autocomplete="off" autocorrect="off" autocapitalize="off">
							<img id="pull_arrow" height="14px;" src="/images/arrow-down.gif" style="position:absolute;*margin-left:-3px;*margin-top:1px;" onclick="pullLANIPList(this);" title="<#select_device_name#>">
							<div id="ClientList_Block_PC" class="clientlist_dropdown"></div>
						</td>
						<td width="25%">
							<input type="text" class="input_18_table" maxlength="18" name="clientlist_dstipAddr" onKeyPress="return validator.isIPAddrPlusNetmask(this, event)" autocomplete="off" autocorrect="off" autocapitalize="off">
						</td>
						<td width="10%">
							<select name="clientlist_iface" class="input_option">
								<option value="WAN">WAN</option>
								<option value="VPN" selected>VPN</option>
							</select>
						</td>
						<td width="12%">
							<div>
								<input type="button" class="add_btn" onClick="addRow_Group(100);" value="">
							</div>
						</td>
					</tr>
				</table>
				<div id="clientlist_Block"></div>

				<table width="100%" border="1" align="center" cellpadding="4" cellspacing="0" class="FormTable_table" style="margin-top:8px;">
					<thead>
						<tr>
							<td><#vpn_openvpn_CustomConf#></td>
						</tr>
					</thead>
					<tr>
						<td>
							<textarea rows="8" class="textarea_ssh_table" spellcheck="false" style="width:99%;" id="vpn_client_custom_x" cols="55" maxlength="2047"></textarea>
						</td>
					</tr>
					</table>
					<div class="apply_gen">
						<input type="button" id="restoreButton" class="button_gen" value="<#Setting_factorydefault_value#>" onclick="defaultSettings();">
						<input name="button" type="button" class="button_gen" onclick="applyRule(0);" value="<#CTL_apply#>"/>
			        </div>
				</td></tr>
	        </tbody>
            </table>
            </form>
            </td>

       </tr>
      </table>
      <!--===================================Ending of Main Content===========================================-->
    </td>
    <td width="10" align="center" valign="top">&nbsp;</td>
  </tr>
</table>
<div id="footer"></div>
</body>
</html>

