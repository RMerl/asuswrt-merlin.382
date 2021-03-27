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
<title><#Web_Title#> - <#menu5_6_2#></title>
<link rel="stylesheet" type="text/css" href="index_style.css"> 
<link rel="stylesheet" type="text/css" href="form_style.css">
<link rel="stylesheet" type="text/css" href="pwdmeter.css">
<link rel="stylesheet" type="text/css" href="device-map/device-map.css">
<link rel="stylesheet" type="text/css" href="css/icon.css">
<script language="JavaScript" type="text/javascript" src="/state.js"></script>
<script language="JavaScript" type="text/javascript" src="/general.js"></script>
<script language="JavaScript" type="text/javascript" src="/popup.js"></script>
<script language="JavaScript" type="text/javascript" src="/help.js"></script>
<script language="JavaScript" type="text/javascript" src="/client_function.js"></script>
<script language="JavaScript" type="text/javascript" src="/validator.js"></script>
<script language="JavaScript" type="text/javascript" src="/js/jquery.js"></script>
<script type="text/javascript" src="/js/httpApi.js"></script>
<style>
.cancel{
	border: 2px solid #898989;
	border-radius:50%;
	background-color: #898989;
}
.check{
	border: 2px solid #093;
	border-radius:50%;
	background-color: #093;
}
.cancel, .check{
	width: 22px;
	height: 22px;
	margin:0 auto;
	transition: .35s;
}
.cancel:active, .check:active{
	transform: scale(1.5,1.5);
	opacity: 0.5;
	transition: 0;
}
.all_enable{
	border: 1px solid #393;
	color: #393;
}
.all_disable{
	border: 1px solid #999;
	color: #999;
}
.contentM_upload{
	position:absolute;
	-webkit-border-radius: 5px;
	-moz-border-radius: 5px;
	border-radius: 5px;
	z-index:500;
	background-color:#2B373B;
	display:none;
	margin-left: 30%;
	top: 1400px;
	width:650px;
}


.Upload_item{
	font-family: Arial, Helvetica, sans-serif;
	font-size: 13px;
	font-weight: bolder;
	color: #FFFFFF;
	margin-left: 15px;
	margin-bottom: 15px;
	margin-top: 15px;
}

.Upload_file{
	background-color:#2B373B;
	color:#FC0;
	*color:#000;
	border:0px;
}
.highlight{
	background: #78535b;
    border: 1px solid #f06767;
}
#NTPList_Block_PC{
	border:1px outset #999;
	background-color:#576D73;
	position:absolute;
	*margin-top:26px;	
	margin-left:2px;
	*margin-left:-353px;
	width:346px;
	text-align:left;	
	height:auto;
	overflow-y:auto;
	z-index:200;
	padding: 1px;
	display:none;
}
#NTPList_Block_PC div{
	background-color:#576D73;
	height:auto;
	*height:20px;
	line-height:20px;
	text-decoration:none;
	font-family: Lucida Console;
	padding-left:2px;
}

#NTPList_Block_PC a{
	background-color:#EFEFEF;
	color:#FFF;
	font-size:12px;
	font-family:Arial, Helvetica, sans-serif;
	text-decoration:none;	
}
#NTPList_Block_PC div:hover{
	background-color:#3366FF;
	color:#FFFFFF;
	cursor:default;
}
#ntp_pull_arrow{
        float:left;
        cursor:pointer;
        border:2px outset #EFEFEF;
        background-color:#CCC;
        padding:3px 2px 4px 0px;
}
</style>
<script>
time_day = uptimeStr.substring(5,7);//Mon, 01 Aug 2011 16:25:44 +0800(1467 secs since boot....
time_mon = uptimeStr.substring(9,12);
time_time = uptimeStr.substring(18,20);
dstoffset = '<% nvram_get("time_zone_dstoff"); %>';
cfg_master = '<% nvram_get("cfg_master"); %>';
ncb_enable = '<% nvram_get("ncb_enable"); %>';
wifison = '<% nvram_get("wifison_ready"); %>';

var orig_shell_timeout_x = Math.floor(parseInt("<% nvram_get("shell_timeout"); %>")/60);
var orig_enable_acc_restriction = '<% nvram_get("enable_acc_restriction"); %>';
var orig_restrict_rulelist_array = [];
if((ntpd_support) && (isSwMode('rt')))
	var orig_ntpd_server_redir = '<% nvram_get("ntpd_server_redir"); %>';
var restrict_rulelist_array = [];
var accounts = [<% get_all_accounts(); %>][0];
for(var i=0; i<accounts.length; i++){
	accounts[i] = decodeURIComponent(accounts[i]);
}
if(accounts.length == 0)
	accounts = ['<% nvram_get("http_username"); %>'];

var header_info = [<% get_header_info(); %>];
var host_name = header_info[0].host;
if(tmo_support)
	var theUrl = "cellspot.router";	
else
	var theUrl = host_name;

if(sw_mode == 3 || (sw_mode == 4))
	theUrl = location.hostname;

var ddns_enable_x = '<% nvram_get("ddns_enable_x"); %>';
var ddns_hostname_x_t = '<% nvram_get("ddns_hostname_x"); %>';
var wan_unit = '<% get_wan_unit(); %>';
if(wan_unit == "0")
	var wan_ipaddr = '<% nvram_get("wan0_ipaddr"); %>';
else
	var wan_ipaddr = '<% nvram_get("wan1_ipaddr"); %>';

var httpd_cert_info = [<% httpd_cert_info(); %>][0];
var uploaded_cert = false;

var le_enable = '<% nvram_get("le_enable"); %>';
var orig_http_enable = '<% nvram_get("http_enable"); %>';
var captcha_support = isSupport("captcha");

var tz_table = {}
$.getJSON("/ajax/tz_db.json", function(data){tz_table = data;})

function initial(){	
	//parse nvram to array
	var parseNvramToArray = function(oriNvram) {
		var parseArray = [];
		var oriNvramRow = decodeURIComponent(oriNvram).split('<');
		for(var i = 0; i < oriNvramRow.length; i += 1) {
			if(oriNvramRow[i] != "") {
				var oriNvramCol = oriNvramRow[i].split('>');
				var eachRuleArray = new Array();
				for(var j = 0; j < oriNvramCol.length; j += 1) {
					eachRuleArray.push(oriNvramCol[j]);
				}
				parseArray.push(eachRuleArray);
			}
		}
		return parseArray;
	};
	orig_restrict_rulelist_array = parseNvramToArray('<% nvram_char_to_ascii("","restrict_rulelist"); %>');
	restrict_rulelist_array = JSON.parse(JSON.stringify(orig_restrict_rulelist_array));

	show_menu();
	httpApi.faqURL("1034294", function(url){document.getElementById("faq").href=url;});
	httpApi.faqURL("1037370", function(url){document.getElementById("ntp_faq").href=url;});
	show_http_clientlist();
	showNTPList();
	display_spec_IP(document.form.http_client.value);

	if(reboot_schedule_support){
		document.form.reboot_date_x_Sun.checked = getDateCheck(document.form.reboot_schedule.value, 0);
		document.form.reboot_date_x_Mon.checked = getDateCheck(document.form.reboot_schedule.value, 1);
		document.form.reboot_date_x_Tue.checked = getDateCheck(document.form.reboot_schedule.value, 2);
		document.form.reboot_date_x_Wed.checked = getDateCheck(document.form.reboot_schedule.value, 3);
		document.form.reboot_date_x_Thu.checked = getDateCheck(document.form.reboot_schedule.value, 4);
		document.form.reboot_date_x_Fri.checked = getDateCheck(document.form.reboot_schedule.value, 5);
		document.form.reboot_date_x_Sat.checked = getDateCheck(document.form.reboot_schedule.value, 6);
		document.form.reboot_time_x_hour.value = getrebootTimeRange(document.form.reboot_schedule.value, 0);
		document.form.reboot_time_x_min.value = getrebootTimeRange(document.form.reboot_schedule.value, 1);
		document.getElementById('reboot_schedule_enable_tr').style.display = "";
		hide_reboot_option('<% nvram_get("reboot_schedule_enable"); %>');
	}
	else{
		document.getElementById('reboot_schedule_enable_tr').style.display = "none";
		document.getElementById('reboot_schedule_date_tr').style.display = "none";
		document.getElementById('reboot_schedule_time_tr').style.display = "none";
	}

	setInterval("corrected_timezone();", 5000);
	load_timezones();
	parse_dstoffset(dstoffset);
	document.form.http_passwd2.value = "";
	
	if(svc_ready == "0")
		document.getElementById('svc_hint_div').style.display = "";

	if(!dualWAN_support) {
		$("#network_monitor_tr").hide();
		document.form.dns_probe_chk.checked = false;
		document.form.wandog_enable_chk.checked = false;
	}
	show_network_monitoring();

	if(!HTTPS_support){
		document.getElementById("http_auth_table").style.display = "none";
	}
	else{
		hide_https_lanport(document.form.http_enable.value);
		hide_https_crt();
		show_cert_details();
	}
	var lanport = '<% nvram_get("http_lanport"); %>';
	if (lanport == '') lanport = '80';
	change_url(lanport, 'http_lan')

	var WPSArray = ['WPS'];
	var ez_mode = httpApi.nvramGet (['btn_ez_mode']).btn_ez_mode;
	var ez_radiotoggle = httpApi.nvramGet (['btn_ez_radiotoggle']).btn_ez_radiotoggle;
	if(!wifi_tog_btn_support && !wifi_hw_sw_support && sw_mode != 2 && sw_mode != 4){
		WPSArray.push('WiFi');
	}

	if(cfg_wps_btn_support){
		WPSArray.push('LED');
	}

	if(WPSArray.length > 1){
		$('#btn_ez_radiotoggle_tr').show();
		for(i=0;i<WPSArray.length;i++){
			$('#btn_ez_' + WPSArray[i]).show();
		}

		if(ez_radiotoggle == '1' && ez_mode == '0'){	// WiFi
			document.form.btn_ez_radiotoggle[1].checked = true;
		}
		else if(ez_radiotoggle == '0' && ez_mode == '1'){	// LED
			document.form.btn_ez_radiotoggle[2].checked = true;
		}
		else{	// WPS
			document.form.btn_ez_radiotoggle[0].checked = true;
		}
	}

	/* MODELDEP */
	if(wifison == 1){
		if(sw_mode == 1 || (sw_mode == 3 && cfg_master == 1))
			document.getElementById("sw_mode_radio_tr").style.display = "";
		if(based_modelid == "MAP-AC2200")
			document.getElementById("ncb_enable_option_tr").style.display = "";
	}
	
	if(sw_mode != 1){
		document.form.sshd_enable.remove(2);
		if (ssh_support && ("<% nvram_get("sshd_enable"); %>" == "1"))
			document.form.sshd_enable.selectedIndex = 1;

		document.getElementById('misc_http_x_tr').style.display = "none";
		hideport(0);
		document.form.misc_http_x.disabled = true;
		document.form.misc_httpsport_x.disabled = true;
		document.form.misc_httpport_x.disabled = true;
		document.getElementById("nat_redirect_enable_tr").style.display = "none";

		if ("<% nvram_get("le_enable"); %>" == "1")
			document.form.le_enable[2].checked = true;
	}
	else
		hideport(document.form.misc_http_x[0].checked);

	document.form.http_username.value = '<% nvram_get("http_username"); %>';
	
	if(ssh_support){
		check_sshd_enable('<% nvram_get("sshd_enable"); %>');
		document.form.sshd_authkeys.value = document.form.sshd_authkeys.value.replace(/>/gm,"\r\n");
	}
	else{
		document.getElementById('sshd_enable_tr').style.display = "none";
		document.getElementById('sshd_port_tr').style.display = "none";
                document.getElementById('sshd_password_tr').style.display = "none";
                document.getElementById('auth_keys_tr').style.display = "none";
	}

	/* MODELDEP */
	if(isSupport("is_ax5400_i1")){
		document.getElementById("ntp_pull_arrow").style.display = "";
	}


	if(isSupport("is_ax5400_i1") || tmo_support){
		document.getElementById("telnetd_sshd_table").style.display = "none";
		document.form.telnetd_enable[0].disabled = true;
		document.form.telnetd_enable[1].disabled = true;
		document.form.sshd_enable.disabled = true;
	}
	else{
		document.getElementById("telnetd_sshd_table").style.display = "";
//		document.form.telnetd_enable[0].disabled = false;
//		document.form.telnetd_enable[1].disabled = false;
		telnet_enable(httpApi.nvramGet(["telnetd_enable"]).telnetd_enable);
	}

	document.getElementById("telnet_tr").style.display = "none";
	document.form.telnetd_enable[0].disabled = true;
	document.form.telnetd_enable[1].disabled = true;

	if(powerline_support)
		document.getElementById("plc_sleep_tr").style.display = "";

	// load shell_timeout_x
	document.form.shell_timeout_x.value = orig_shell_timeout_x;

	if(pwrsave_support){
		document.getElementById("pwrsave_tr").style.display = "";
		document.form.pwrsave_mode[0].disabled = false;
		document.form.pwrsave_mode[1].disabled = false;
	}
	else{
		document.getElementById("pwrsave_tr").style.display = "none";
		document.form.pwrsave_mode[0].disabled = false;
		document.form.pwrsave_mode[1].disabled = false;
	}

	if(hdspindown_support) {
		$("#hdd_spindown_table").css("display", "");
		change_hddSpinDown($('select[name="usb_idle_enable"]').val());
		$('select[name="usb_idle_enable"]').prop("disabled", false);
		$('input[name="usb_idle_timeout"]').prop("disabled", false);

		if (document.form.usb_idle_exclude.value.indexOf("a") != -1)
			document.form.usb_idle_exclude_a.checked = true;
		if (document.form.usb_idle_exclude.value.indexOf("b") != -1)
			document.form.usb_idle_exclude_b.checked = true;
		if (document.form.usb_idle_exclude.value.indexOf("c") != -1)
			document.form.usb_idle_exclude_c.checked = true;
		if (document.form.usb_idle_exclude.value.indexOf("d") != -1)
			document.form.usb_idle_exclude_d.checked = true;
		if (document.form.usb_idle_exclude.value.indexOf("e") != -1)
			document.form.usb_idle_exclude_e.checked = true;
		if (document.form.usb_idle_exclude.value.indexOf("f") != -1)
			document.form.usb_idle_exclude_f.checked = true;
		if (document.form.usb_idle_exclude.value.indexOf("g") != -1)
			document.form.usb_idle_exclude_g.checked = true;
		if (document.form.usb_idle_exclude.value.indexOf("h") != -1)
			document.form.usb_idle_exclude_h.checked = true;
		if (document.form.usb_idle_exclude.value.indexOf("i") != -1)
			document.form.usb_idle_exclude_i.checked = true;
	}

	if ( (ntpd_support) && (isSwMode('rt')) )
		showhide("ntpd_redir_tr", '<% nvram_get("ntpd_enable"); %>');
	else {
		showhide("ntpd_server_tr", 0);
		showhide("ntpd_redir_tr", 0);
	}

	$("#https_download_cert").css("display", (le_enable != "1" && orig_http_enable != "0")? "": "none");

	$("#login_captcha_tr").css("display", captcha_support? "": "none");
}

var time_zone_tmp="";
var time_zone_s_tmp="";
var time_zone_e_tmp="";
var time_zone_withdst="";

function applyRule(){
	if(validForm()){
		var isFromHTTPS = (function(){
			if(location.protocol.toLowerCase() == "https:") return true;
			else return false;
		})();

		var isFromWAN = (function(){
			var lanIpAddr = '<% nvram_get("lan_ipaddr"); %>';
			if(location.hostname == lanIpAddr) return false;
			else if(location.hostname == "router.asus.com") return false;
			else if(location.hostname == "repeater.asus.com") return false;
			else if(location.hostname == "cellspot.asus.com") return false;
			else return true;
		})();

		var restart_firewall_flag = false;
		var restart_httpd_flag = false;
		var ncb_enable_option_flag = false;
		var sw_mode_radio_flag = false;
	
		//parse array to nvram
		var old_fw_tmp_value = ""; //for old version fw
		var tmp_value = "";
		for(var i = 0; i < restrict_rulelist_array.length; i += 1) {
			if(restrict_rulelist_array[i].length != 0) {
				old_fw_tmp_value += "<";
				tmp_value += "<";
				for(var j = 0; j < restrict_rulelist_array[i].length; j += 1) {
					tmp_value += restrict_rulelist_array[i][j];
					if( (j + 1) != restrict_rulelist_array[i].length)
						tmp_value += ">";

					if(j == 1) //IP, for old version fw
						old_fw_tmp_value += restrict_rulelist_array[i][j];
				}
			}
		}

		var getRadioItemCheck = function(obj) {
			var checkValue = "";
			var radioLength = obj.length;
			for(var i = 0; i < radioLength; i += 1) {
				if(obj[i].checked) {
					checkValue = obj[i].value;
					break;
				}
			}
			return 	checkValue;
		};
		document.form.http_client.value = getRadioItemCheck(document.form.http_client_radio); //for old version fw
		document.form.http_clientlist.value = old_fw_tmp_value; //for old version fw
		document.form.enable_acc_restriction.value = getRadioItemCheck(document.form.http_client_radio);
		document.form.restrict_rulelist.value = tmp_value;
	
		if(document.form.restrict_rulelist.value == "" && document.form.http_client_radio[0].checked == 1){
			alert("<#JS_fieldblank#>");
			document.form.http_client_ip_x_0.focus();
			return false;
		}

//		if((document.form.enable_acc_restriction.value != orig_enable_acc_restriction) || (restrict_rulelist_array.toString() != orig_restrict_rulelist_array.toString())
//			|| (document.form.ntpd_server_redir.value != orig_ntpd_server_redir) )
			restart_firewall_flag = true;

		if(document.form.http_passwd2.value.length > 0){
			document.form.http_passwd.value = document.form.http_passwd2.value;
			document.form.http_passwd.disabled = false;
		}
		
		if(document.form.time_zone_select.value.search("DST") >= 0 || document.form.time_zone_select.value.search("TDT") >= 0){		// DST area

				time_zone_tmp = document.form.time_zone_select.value.split("_");	//0:time_zone 1:serial number
				time_zone_s_tmp = "M"+document.form.dst_start_m.value+"."+document.form.dst_start_w.value+"."+document.form.dst_start_d.value+"/"+document.form.dst_start_h.value;
				time_zone_e_tmp = "M"+document.form.dst_end_m.value+"."+document.form.dst_end_w.value+"."+document.form.dst_end_d.value+"/"+document.form.dst_end_h.value;
				document.form.time_zone_dstoff.value=time_zone_s_tmp+","+time_zone_e_tmp;
				document.form.time_zone.value = document.form.time_zone_select.value;
		}else{
				//document.form.time_zone_dstoff.value="";	//Don't change time_zone_dstoff vale
				document.form.time_zone.value = document.form.time_zone_select.value;
		}

		// shell_timeout save min to sec
		document.form.shell_timeout.value = parseInt(document.form.shell_timeout_x.value)*60;
		
		if(document.form.misc_http_x[1].checked == true){
				document.form.misc_httpport_x.disabled = true;
				document.form.misc_httpsport_x.disabled = true;
		}		
		if(document.form.misc_http_x[0].checked == true 
				&& document.form.http_enable[0].selected == true){
				document.form.misc_httpsport_x.disabled = true;
		}	
		if(document.form.misc_http_x[0].checked == true 
				&& document.form.http_enable[1].selected == true){
				document.form.misc_httpport_x.disabled = true;
		}

		if(document.form.http_lanport.value != '<% nvram_get("http_lanport"); %>'
				|| document.form.https_lanport.value != '<% nvram_get("https_lanport"); %>'
				|| document.form.http_enable.value != '<% nvram_get("http_enable"); %>'
				|| document.form.misc_httpport_x.value != '<% nvram_get("misc_httpport_x"); %>'
				|| document.form.misc_httpsport_x.value != '<% nvram_get("misc_httpsport_x"); %>'
				|| getRadioItemCheck(document.form.https_crt_gen) == "1"
		                || uploaded_cert
				|| document.form.https_crt_cn.value != '<% nvram_get("https_crt_cn"); %>'
			){
			restart_httpd_flag = true;
			if(document.form.https_crt_cn.value != '<% nvram_get("https_crt_cn"); %>'){
				document.form.https_crt_gen.value = "1";
			}
			if(document.form.http_enable.value == "0"){	//HTTP
				if(isFromWAN)
					document.form.flag.value = "http://" + location.hostname + ":" + document.form.misc_httpport_x.value;
				else if (document.form.http_lanport.value)
					document.form.flag.value = "http://" + location.hostname + ":" + document.form.http_lanport.value;
				else
					document.form.flag.value = "http://" + location.hostname;
			}
			else if(document.form.http_enable.value == "1"){	//HTTPS
				if(isFromWAN)
					document.form.flag.value = "https://" + location.hostname + ":" + document.form.misc_httpsport_x.value;
				else
					document.form.flag.value = "https://" + location.hostname + ":" + document.form.https_lanport.value;
			}
			else{	//BOTH
				if(isFromHTTPS){
					if(isFromWAN)
						document.form.flag.value = "https://" + location.hostname + ":" + document.form.misc_httpsport_x.value;
					else
						document.form.flag.value = "https://" + location.hostname + ":" + document.form.https_lanport.value;
				}else{
					if(isFromWAN)
						document.form.flag.value = "http://" + location.hostname + ":" + document.form.misc_httpport_x.value;
					else if (document.form.http_lanport.value)
						document.form.flag.value = "http://" + location.hostname + ":" + document.form.http_lanport.value;
					else
						document.form.flag.value = "http://" + location.hostname;
				}
			}
		}

		if(ncb_enable != "" && document.form.ncb_enable_option.value != ncb_enable){
			document.form.ncb_enable.value = document.form.ncb_enable_option.value;
			ncb_enable_option_flag = true;
		}

		if((document.form.sw_mode_radio.value==1 && sw_mode!=3) ||
			(document.form.sw_mode_radio.value==0 && sw_mode==3) ){
			if (sw_mode == 1) document.form.sw_mode.value = 3;
			else if (sw_mode == 3) document.form.sw_mode.value = 1;

			sw_mode_radio_flag = true;
		}

		if(document.form.btn_ez_radiotoggle[1].checked){	// WiFi
			document.form.btn_ez_radiotoggle.value = 1;
			document.form.btn_ez_mode.value = 0;
		}
		else if(document.form.btn_ez_radiotoggle[2].checked){	// LED
			document.form.btn_ez_radiotoggle.value = 0;
			document.form.btn_ez_mode.value = 1;
		}
		else{	// WPS
			document.form.btn_ez_radiotoggle.value = 0;
			document.form.btn_ez_mode.value = 0;
		}
		
		if(reboot_schedule_support){
			updateDateTime();
		}

		if(document.form.wandog_enable_chk.checked)
			document.form.wandog_enable.value = "1";
		else
			document.form.wandog_enable.value = "0";

		if(document.form.dns_probe_chk.checked)
			document.form.dns_probe.value = "1";
		else
			document.form.dns_probe.value = "0";

		if(document.form.https_lanport.value != '<% nvram_get("https_lanport"); %>')
			alert('<#Change_HttpsLanport_Hint#>');

		showLoading();

		var action_script_tmp = "restart_time;restart_leds;";

		if(hdspindown_support) {
			var excluded = "";
			if (document.form.usb_idle_exclude_a.checked)
				excluded += "a";
		        if (document.form.usb_idle_exclude_b.checked)
		                excluded += "b";
		        if (document.form.usb_idle_exclude_c.checked)
		                excluded += "c";
			if (document.form.usb_idle_exclude_d.checked)
				excluded += "d";
			if (document.form.usb_idle_exclude_e.checked)
				excluded += "e";
			if (document.form.usb_idle_exclude_f.checked)
				excluded += "f";
			if (document.form.usb_idle_exclude_g.checked)
				excluded += "g";
			if (document.form.usb_idle_exclude_h.checked)
				excluded += "h";
			if (document.form.usb_idle_exclude_i.checked)
				excluded += "i";

			document.form.usb_idle_exclude.value = excluded;

			action_script_tmp += "restart_usb_idle;";
		}

		if(restart_httpd_flag) {
			action_script_tmp += "restart_httpd;";

			if ((getRadioItemCheck(document.form.https_crt_gen) == "1"
				|| uploaded_cert
				|| document.form.https_crt_cn.value != '<% nvram_get("https_crt_cn"); %>')
				&& ('<% nvram_get("enable_ftp"); %>' == "1")
				&& ('<% nvram_get("ftp_tls"); %>' == "1")) {
					action_script_tmp += ";restart_ftpd";
			}
		}

		if(restart_firewall_flag)
			action_script_tmp += "restart_firewall;";
		else
			action_script_tmp += "restart_upnp;";	// Normally done by restart_firewall


		if(ncb_enable_option_flag)
			action_script_tmp += "restart_bhblock;";

		if(sw_mode_radio_flag) {
			action_script_tmp += "restart_chg_swmode;";
			document.form.action_wait.value = 210;
		}

		if(pwrsave_support)
			action_script_tmp += "pwrsave;";

		if(needReboot){
			action_script_tmp = "reboot";
			document.form.action_wait.value = httpApi.hookGet("get_default_reboot_time");
		}

		if (getRadioItemCheck(document.form.ntpd_enable) != '<% nvram_get("ntpd_enable"); %>')
			action_script_tmp += "restart_dnsmasq;";

		document.form.action_script.value = action_script_tmp;
		document.form.submit();
	}
}

function validForm(){
	showtext(document.getElementById("alert_msg1"), "");
	showtext(document.getElementById("alert_msg2"), "");

	if(document.form.http_username.value.length == 0){
		showtext(document.getElementById("alert_msg1"), "<#File_Pop_content_alert_desc1#>");
		document.form.http_username.focus();
		document.form.http_username.select();
		return false;
	}
	else{
		var alert_str = validator.account_name(document.form.http_username);

		if(alert_str != ""){
			showtext(document.getElementById("alert_msg1"), alert_str);
			document.getElementById("alert_msg1").style.display = "";
			document.form.http_username.focus();
			document.form.http_username.select();
			return false;
		}else{
			document.getElementById("alert_msg1").style.display = "none";
		}

		document.form.http_username.value = trim(document.form.http_username.value);

		if(document.form.http_username.value == "root"
				|| document.form.http_username.value == "guest"
				|| document.form.http_username.value == "anonymous"
		){
				showtext(document.getElementById("alert_msg1"), "<#USB_Application_account_alert#>");
				document.getElementById("alert_msg1").style.display = "";
				document.form.http_username.focus();
				document.form.http_username.select();
				return false;
		}
		else if(accounts.getIndexByValue(document.form.http_username.value) > 0
				&& document.form.http_username.value != accounts[0]){
				showtext(document.getElementById("alert_msg1"), "<#File_Pop_content_alert_desc5#>");
				document.getElementById("alert_msg1").style.display = "";
				document.form.http_username.focus();
				document.form.http_username.select();
				return false;
		}else{
				document.getElementById("alert_msg1").style.display = "none";
		}
	}

	if(document.form.http_passwd2.value.length > 0 && document.form.http_passwd2.value.length < 5){
		showtext(document.getElementById("alert_msg2"),"* <#JS_short_password#>");
		document.form.http_passwd2.focus();
		document.form.http_passwd2.select();
		return false;
	}

	if(document.form.http_passwd2.value.length > 16){
		showtext(document.getElementById("alert_msg2"),"* <#JS_max_password#>");
		document.form.http_passwd2.focus();
		document.form.http_passwd2.select();
		return false;
	}

	if(document.form.http_passwd2.value != document.form.v_password2.value){
		showtext(document.getElementById("alert_msg2"),"* <#File_Pop_content_alert_desc7#>");
		if(document.form.http_passwd2.value.length <= 0){
			document.form.http_passwd2.focus();
			document.form.http_passwd2.select();
		}else{
			document.form.v_password2.focus();
			document.form.v_password2.select();
		}
		return false;
	}

	if(is_KR_sku){		/* MODELDEP by Territory Code */
		if(document.form.http_passwd2.value.length > 0 || document.form.http_passwd2.value.length > 0){
			if(!validator.string_KR(document.form.http_passwd2)){
				document.form.http_passwd2.focus();
				document.form.http_passwd2.select();
				return false;
			}
		}
	}
	else{
		if(!validator.string(document.form.http_passwd2)){
			document.form.http_passwd2.focus();
			document.form.http_passwd2.select();
			return false;
		}
	}

	if(document.form.http_passwd2.value == '<% nvram_default_get("http_passwd"); %>'){
		showtext(document.getElementById("alert_msg2"),"* <#QIS_adminpass_confirm0#>");
		document.form.http_passwd2.focus();
		document.form.http_passwd2.select();
		return false;
	}	
	
	//confirm common string combination	#JS_common_passwd#
	var is_common_string = check_common_string(document.form.http_passwd2.value, "httpd_password");
	if(document.form.http_passwd2.value.length > 0 && is_common_string){
			if(!confirm("<#JS_common_passwd#>")){
				document.form.http_passwd2.focus();
				document.form.http_passwd2.select();
				return false;
			}
	}

	if(hdspindown_support) {
		if($('select[name="usb_idle_enable"]').val() == 1) {
			$('input[name="usb_idle_timeout"]').prop("disabled", false);
			if (!validator.range($('input[name="usb_idle_timeout"]')[0], 60, 9999))
				return false;
		}
		else {
			$('input[name="usb_idle_timeout"]').prop("disabled", true);
		}
	}
	
	if((document.form.time_zone_select.value.search("DST") >= 0 || document.form.time_zone_select.value.search("TDT") >= 0)			// DST area
			&& document.form.dst_start_m.value == document.form.dst_end_m.value
			&& document.form.dst_start_w.value == document.form.dst_end_w.value
			&& document.form.dst_start_d.value == document.form.dst_end_d.value){
		alert("<#FirewallConfig_URLActiveTime_itemhint4#>");	//At same day
		document.form.dst_start_m.focus();
		return false;
	}

	if(document.form.sshd_enable.value != "0" && document.form.sshd_pass[1].checked && document.form.sshd_authkeys.value == ""){		
		alert("<#JS_fieldblank#>");
		document.form.sshd_authkeys.focus();
		return false;
	}

	if(document.form.sshd_enable.value != 0){
		if (!validator.range(document.form.sshd_port, 1, 65535))
			return false;
		else if(myisPortConflict(document.form.sshd_port.value, "ssh")){
			alert(myisPortConflict(document.form.sshd_port.value, "ssh"));
			document.form.sshd_port.focus();
			return false;
		}
	}
	else{
		document.form.sshd_port.disabled = true;
	}

	if (!validator.range(document.form.http_lanport, 1, 65535))
		/*return false;*/ document.form.http_lanport = 80;
	if (HTTPS_support && !document.form.https_lanport.disabled && !validator.range(document.form.https_lanport, 1, 65535) && !tmo_support)
		return false;

	if (document.form.misc_http_x[0].checked) {
		if (!document.form.misc_httpport_x.disabled && !validator.range(document.form.misc_httpport_x, 1, 65535))
			return false;
		if (HTTPS_support && !document.form.misc_httpsport_x.disabled && !validator.range(document.form.misc_httpsport_x, 1, 65535))
			return false;
	}
	else{
		document.form.misc_httpport_x.value = '<% nvram_get("misc_httpport_x"); %>';
		document.form.misc_httpsport_x.value = '<% nvram_get("misc_httpsport_x"); %>';
	}

/* redundant
	if(document.form.sshd_port.value == document.form.https_lanport.value){
		alert("<#SSH_HttpsLanPort_Conflict_Hint#>");
		$("#sshd_port").addClass("highlight");
		$("#port_conflict_sshdport").show();
		$("#https_lanport_input").addClass("highlight");
		$("#port_conflict_httpslanport").show();
		document.form.sshd_port.focus();
		return false;
	}
*/
	if(!validator.rangeAllowZero(document.form.shell_timeout_x, 10, 999, orig_shell_timeout_x))
		return false;

	if((document.form.sshd_enable.value != 0) && (document.form.sshd_authkeys.value.length == 0) && (!document.form.sshd_pass[0].checked)){
		alert("You must configure at least one SSH authentication method!");
		return false;
	}

/* HTTP WAN access is no longer allowed */
/*
	if(!document.form.misc_httpport_x.disabled &&
			myisPortConflict(document.form.misc_httpport_x.value, "http")){
		alert(isPortConflict(document.form.misc_httpport_x.value, "http"));
		document.form.misc_httpport_x.focus();
		document.form.misc_httpport_x.select();
		return false;
	}
*/
	if(!document.form.misc_httpsport_x.disabled &&
			myisPortConflict(document.form.misc_httpsport_x.value, "https") && HTTPS_support){
		alert(myisPortConflict(document.form.misc_httpsport_x.value, "https"));
		document.form.misc_httpsport_x.focus();
		document.form.misc_httpsport_x.select();
		return false;
	}
	else if(myisPortConflict(document.form.http_lanport.value, "http")){
		alert(isPortConflict(document.form.http_lanport.value, "http"));
		document.form.http_lanport.focus();
		document.form.http_lanport.select();
		return false;
	}
	else if(!document.form.https_lanport.disabled && myisPortConflict(document.form.https_lanport.value, "https") && HTTPS_support){
		alert(isPortConflict(document.form.https_lanport.value, "https"));
		document.form.https_lanport.focus();
		document.form.https_lanport.select();
		return false;
	}

	else if(!validator.rangeAllowZero(document.form.http_autologout, 10, 999, '<% nvram_get("http_autologout"); %>'))
		return false;

	if(reboot_schedule_support){
		if(!document.form.reboot_date_x_Sun.checked && !document.form.reboot_date_x_Mon.checked &&
		!document.form.reboot_date_x_Tue.checked && !document.form.reboot_date_x_Wed.checked &&
		!document.form.reboot_date_x_Thu.checked && !document.form.reboot_date_x_Fri.checked &&
		!document.form.reboot_date_x_Sat.checked && document.form.reboot_schedule_enable_x[0].checked)
		{
			alert(Untranslated.filter_lw_date_valid);
			document.form.reboot_date_x_Sun.focus();
			return false;
		}
	}

	if(document.form.http_passwd2.value.length > 0)	//password setting changed
		alert("<#File_Pop_content_alert_desc10#>");
	
	//Not allowed no Web UI in restrict_rulelist_array
	var WebUI_selected=0
	if(document.form.http_client_radio[0].checked && restrict_rulelist_array.length >0){  //Allow only specified IP address
		for(var x=0;x<restrict_rulelist_array.length;x++){
			if(restrict_rulelist_array[x][0] == 1 &&        //enabled rule && Web UI included
				(restrict_rulelist_array[x][2] == 1 || restrict_rulelist_array[x][2] == 3 || restrict_rulelist_array[x][2] == 5 || restrict_rulelist_array[x][2] == 7)){
				WebUI_selected++;
			}
		}

		if(WebUI_selected <= 0){
			alert("<#JS_access_type#> <#System_login_specified_Iplist_enable#>");
			document.form.http_client_ip_x_0.focus();
			return false;
		}
	}
	
	return true;
}

function done_validating(action){
	refreshpage();
}

function corrected_timezone(){
	var today = new Date();
	var StrIndex;
	var timezone = uptimeStr_update.substring(26,31);
	var startPos = today.toString().indexOf("(");
	if ( startPos < 0 )
		startPos = today.toString().length;

	if(today.toString().lastIndexOf("-", startPos) > 0)
		StrIndex = today.toString().lastIndexOf("-", startPos);
	else if(today.toString().lastIndexOf("+", startPos) > 0)
		StrIndex = today.toString().lastIndexOf("+", startPos);

	if(StrIndex > 0){
		//alert('dstoffset='+dstoffset+', 設定時區='+timezone+' , 當地時區='+today.toString().substring(StrIndex, StrIndex+5))
		if(timezone != today.toString().substring(StrIndex, StrIndex+5)){
			document.getElementById("timezone_hint").style.display = "block";
			document.getElementById("timezone_hint").innerHTML = "* <#LANHostConfig_x_TimeZone_itemhint#>";
		}
		else
			return;
	}
	else
		return;
}

var timezones = [
	["UTC12",	"(GMT-12:00) <#TZ01#>"],
	["UTC11",	"(GMT-11:00) <#TZ02#>"],
	["UTC10",	"(GMT-10:00) <#TZ03#>"],
	["NAST9DST",	"(GMT-09:00) <#TZ04#>"],
	["PST8DST",	"(GMT-08:00) <#TZ05#>"],
	["MST7DST_1",	"(GMT-07:00) <#TZ06#>"],
	["MST7_2",	"(GMT-07:00) <#TZ07#>"],
	["MST7DST_3",	"(GMT-07:00) <#TZ08#>"],
	["CST6_2",	"(GMT-06:00) <#TZ10#>"],
	["CST6DST_3",	"(GMT-06:00) <#TZ11#>"],
	["CST6DST_3_1",	"(GMT-06:00) <#TZ12#>"],
	["UTC6DST",	"(GMT-06:00) <#TZ13#>"],
	["EST5DST",	"(GMT-05:00) <#TZ14#>"],
	["UTC5_1",	"(GMT-05:00) <#TZ15#>"],
	["UTC5_2",	"(GMT-05:00) <#TZ16#>"],
	["AST4DST",	"(GMT-04:00) <#TZ17#>"],
	["UTC4_1",	"(GMT-04:00) <#TZ18#>"],
	["UTC4_2",	"(GMT-04:00) <#TZ18_1#>"],
	["UTC4DST_2",	"(GMT-04:00) <#TZ19#>"],
	["NST3.30DST",	"(GMT-03:30) <#TZ20#>"],
	["EBST3",	"(GMT-03:00) <#TZ21#>"],	//EBST3DST_1
	["UTC3",	"(GMT-03:00) <#TZ22#>"],
	["EBST3DST_2",	"(GMT-03:00) <#TZ23#>"],
	["UTC2",	"(GMT-02:00) <#TZ24#>"],
	["EUT1DST",	"(GMT-01:00) <#TZ25#>"],
	["UTC1",	"(GMT-01:00) <#TZ26#>"],
	["GMT0",	"(GMT) <#TZ27#>"],
	["GMT0DST_1",	"(GMT) <#TZ27_2#>"],
	["GMT0DST_2",	"(GMT) <#TZ28#>"],
	["GMT0_2",	"(GMT) <#TZ28_1#>"],
	["UTC-1DST_1",	"(GMT+01:00) <#TZ29#>"],
	["UTC-1DST_1_1","(GMT+01:00) <#TZ30#>"],
	["UTC-1DST_1_2",	"(GMT+01:00) <#TZ31#>"],
	["UTC-1DST_2",	"(GMT+01:00) <#TZ32#>"],
	["MET-1DST",	"(GMT+01:00) <#TZ33#>"],
	["MET-1DST_1",	"(GMT+01:00) <#TZ34#>"],
	["MEZ-1DST",	"(GMT+01:00) <#TZ35#>"],
	["MEZ-1DST_1",	"(GMT+01:00) <#TZ36#>"],
	["UTC-1_3",	"(GMT+01:00) <#TZ37#>"],
	["UTC-2DST",	"(GMT+02:00) <#TZ38#>"],
	["UTC-2DST_3",	"(GMT+02:00) <#TZ33_1#>"],
	["EST-2",	"(GMT+02:00) <#TZ39#>"],
	["UTC-2DST_4",	"(GMT+02:00) <#TZ40#>"],
	["UTC-2DST_2",	"(GMT+02:00) <#TZ41#>"],
	["IST-2DST",	"(GMT+02:00) <#TZ42#>"],
	["EET-2DST",	"(GMT+02:00) <#TZ43_2#>"],
	["UTC-2_1",	"(GMT+02:00) <#TZ40_2#>"],
	["SAST-2",	"(GMT+02:00) <#TZ43#>"],
	["UTC-3_1",	"(GMT+03:00) <#TZ46#>"],
	["UTC-3_2",	"(GMT+03:00) <#TZ47#>"],
	["UTC-3_3",	"(GMT+03:00) <#TZ40_1#>"],
	["UTC-3_4",	"(GMT+03:00) <#TZ44#>"],	
	["IST-3",	"(GMT+03:00) <#TZ48#>"],
	["UTC-3_6",	"(GMT+03:00) <#TZ48_1#>"],
	["UTC-3.30DST",	"(GMT+03:30) <#TZ49#>"],	
	["UTC-4_1",	"(GMT+04:00) <#TZ50#>"],
	["UTC-4_5",	"(GMT+04:00) <#TZ50_2#>"],
	["UTC-4_7",	"(GMT+04:00) <#TZ45#>"],	//UTC-3_5
	["UTC-4_4",	"(GMT+04:00) <#TZ50_1#>"],
	["UTC-4_6",	"(GMT+04:00) <#TZ51#>"],
	["UTC-4.30",	"(GMT+04:30) <#TZ52#>"],
	["UTC-5",	"(GMT+05:00) <#TZ54#>"],
	["UTC-5_1",	"(GMT+05:00) <#TZ53#>"],
	["UTC-5.30_2",	"(GMT+05:30) <#TZ55#>"],
	["UTC-5.30_1",	"(GMT+05:30) <#TZ56#>"],
	["UTC-5.30",	"(GMT+05:30) <#TZ59#>"],
	["UTC-5.45",	"(GMT+05:45) <#TZ57#>"],
	["RFT-6",	"(GMT+06:00) <#TZ60#>"],
	["UTC-6",	"(GMT+06:00) <#TZ58#>"],
	["UTC-6.30",	"(GMT+06:30) <#TZ61#>"],
	["UTC-7",	"(GMT+07:00) <#TZ62#>"],
	["UTC-7_2",	"(GMT+07:00) <#TZ63#>"],
	["UTC-7_3",     "(GMT+07:00) <#TZ62_1#>"],      //UTC-6_2
	["CST-8",	"(GMT+08:00) <#TZ64#>"],
	["CST-8_1",	"(GMT+08:00) <#TZ65#>"],
	["SST-8",	"(GMT+08:00) <#TZ66#>"],
	["CCT-8",	"(GMT+08:00) <#TZ67#>"],
	["WAS-8",	"(GMT+08:00) <#TZ68#>"],
	["UTC-8",	"(GMT+08:00) <#TZ69#>"],
	["UTC-8_1",     "(GMT+08:00) <#TZ70#>"],
	["UTC-9_1",	"(GMT+09:00) <#TZ70_1#>"],
	["UTC-9_3",	"(GMT+09:00) <#TZ72#>"],
	["JST",		"(GMT+09:00) <#TZ71#>"],
	["CST-9.30",	"(GMT+09:30) <#TZ73#>"],
	["UTC-9.30DST",	"(GMT+09:30) <#TZ74#>"],
	["UTC-10DST_1",	"(GMT+10:00) <#TZ75#>"],
	["UTC-10_2",	"(GMT+10:00) <#TZ76#>"],
	["UTC-10_4",	"(GMT+10:00) <#TZ78#>"],	
	["TST-10TDT",	"(GMT+10:00) <#TZ77#>"],
	["UTC-10_6",	"(GMT+10:00) <#TZ79#>"],
	["UTC-11",	"(GMT+11:00) <#TZ80#>"],
	["UTC-11_1",	"(GMT+11:00) <#TZ81#>"],
	["UTC-11_3",	"(GMT+11:00) <#TZ86#>"],
	["UTC-11_4",	"(GMT+11:00) <#TZ82_1#>"],
	["UTC-12",      "(GMT+12:00) <#TZ82#>"],
	["UTC-12_2",      "(GMT+12:00) <#TZ85#>"],
	["NZST-12DST",	"(GMT+12:00) <#TZ83#>"],
	["UTC-13",	"(GMT+13:00) <#TZ84#>"]];

function load_timezones(){
	free_options(document.form.time_zone_select);
	for(var i = 0; i < timezones.length; i++){
		add_option(document.form.time_zone_select,
			timezones[i][1], timezones[i][0],
			(document.form.time_zone.value == timezones[i][0]));
	}
	select_time_zone();	
}

var timezone_dst_changes = {
	"NAST9DST":	[ 3,2,0,2,  11,1,0,2 ],
	"PST8DST":	[ 3,2,0,2,  11,1,0,2 ],
	"MST7DST_1":	[ 3,2,0,2,  11,1,0,2 ],
	"MST7DST_3":	[ 4,1,0,2,  10,5,0,2 ],
	"CST6DST_3":	[ 4,1,0,2,  10,5,0,2 ],
	"CST6DST_3_1":	[ 4,1,0,2,  10,5,0,2 ],
	"UTC6DST":	[ 3,2,0,2,  11,1,0,2 ],
	"EST5DST":	[ 3,2,0,2,  11,1,0,2 ],
	"AST4DST":	[ 3,2,0,2,  11,1,0,2 ],
	"UTC4DST_2":	[ 8,2,0,0,   5,2,0,0 ],
	"NST3.30DST":	[ 3,2,0,2,  11,1,0,2 ],
	"EBST3DST_1":	[11,1,0,0,   2,3,0,0 ],
	"EBST3DST_2":	[ 3,5,6,22, 10,5,6,23],
	"EUT1DST":	[ 3,5,0,0,  10,5,0,1 ],
	"GMT0DST_1":	[ 3,5,0,1,  10,5,0,2 ],
	"GMT0DST_2":	[ 3,5,0,2,  10,5,0,3 ],
	"UTC-1DST_1":	[ 3,5,0,2,  10,5,0,3 ],
	"UTC-1DST_1_1":	[ 3,5,0,2,  10,5,0,3 ],
	"UTC-1DST_1_2":	[ 3,5,0,2,  10,5,0,3 ],
	"UTC-1DST_2":	[ 3,5,0,2,  10,5,0,3 ],
	"MET-1DST":	[ 3,5,0,2,  10,5,0,3 ],
	"MET-1DST_1":	[ 3,5,0,2,  10,5,0,3 ],
	"MEZ-1DST":	[ 3,5,0,2,  10,5,0,3 ],
	"MEZ-1DST_1":	[ 3,5,0,2,  10,5,0,3 ],
	"UTC-2DST":	[ 3,5,0,3,  10,5,0,4 ],
	"UTC-2DST_3":	[ 3,5,0,3,  10,5,0,4 ],
	"UTC-2DST_4":	[ 3,5,0,3,  10,5,0,4 ],
	"UTC-2DST_2":	[ 3,5,0,3,  10,5,0,4 ],
	"IST-2DST":	[ 3,5,5,2,  10,5,0,2 ],
	"EET-2DST":	[ 3,5,0,3,  10,5,0,4 ],
	"UTC-9.30DST":	[10,1,0,2,   4,1,0,3 ],
	"UTC-10DST_1":	[10,1,0,2,   4,1,0,3 ],
	"TST-10TDT":	[10,1,0,2,   4,1,0,3 ],
	"NZST-12DST":	[ 9,5,0,2,   4,1,0,3 ]
};

function autofill_dst(){
	if (document.form.time_zone_select.value.search("DST") >= 0 || document.form.time_zone_select.value.search("TDT") >= 0) {
		if (timezone_dst_changes[document.form.time_zone_select.value]) {
			document.form.dst_start_m.value = timezone_dst_changes[document.form.time_zone_select.value][0];
			document.form.dst_start_w.value = timezone_dst_changes[document.form.time_zone_select.value][1];
			document.form.dst_start_d.value = timezone_dst_changes[document.form.time_zone_select.value][2];
			document.form.dst_start_h.value = timezone_dst_changes[document.form.time_zone_select.value][3];
			document.form.dst_end_m.value = timezone_dst_changes[document.form.time_zone_select.value][4];
			document.form.dst_end_w.value = timezone_dst_changes[document.form.time_zone_select.value][5];
			document.form.dst_end_d.value = timezone_dst_changes[document.form.time_zone_select.value][6];
			document.form.dst_end_h.value = timezone_dst_changes[document.form.time_zone_select.value][7];
		}
	}
}

var dst_month = new Array("", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12");
var dst_week = new Array("", "1st", "2nd", "3rd", "4th", "5th");
var dst_day = new Array("<#date_Sun_itemdesc#>", "<#date_Mon_itemdesc#>", "<#date_Tue_itemdesc#>", "<#date_Wed_itemdesc#>", "<#date_Thu_itemdesc#>", "<#date_Fri_itemdesc#>", "<#date_Sat_itemdesc#>");
var dst_hour = new Array("0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12",
													"13", "14", "15", "16", "17", "18", "19", "20", "21", "22", "23");
var dstoff_start_m,dstoff_start_w,dstoff_start_d,dstoff_start_h;
var dstoff_end_m,dstoff_end_w,dstoff_end_d,dstoff_end_h;

function parse_dstoffset(_dstoffset){     //Mm.w.d/h,Mm.w.d/h
	if(_dstoffset){
		var dstoffset_startend = _dstoffset.split(",");
			
		if(dstoffset_startend[0] != "" && dstoffset_startend[0] != undefined){
			var dstoffset_start = trim(dstoffset_startend[0]);
			var dstoff_start = dstoffset_start.split(".");
			
			dstoff_start_m = parseInt(dstoff_start[0].substring(1));
			if(check_range(dstoff_start_m,1,12)){
				document.form.dst_start_m.value = dstoff_start_m;
			}

			if(dstoff_start[1] != "" && dstoff_start[1] != undefined){
				dstoff_start_w = parseInt(dstoff_start[1]);
				if(check_range(dstoff_start_w,1,5)){
					document.form.dst_start_w.value = dstoff_start_w;
				}
			}

			if(dstoff_start[2] != "" && dstoff_start[2] != undefined){
				dstoff_start_d = parseInt(dstoff_start[2].split("/")[0]);
				if(check_range(dstoff_start_d,0,6)){
					document.form.dst_start_d.value = dstoff_start_d;
				}

				dstoff_start_h = parseInt(dstoff_start[2].split("/")[1]);
				if(check_range(dstoff_start_h,0,23)){
					document.form.dst_start_h.value = dstoff_start_h;
				}
			}
		}
		
		if(dstoffset_startend[1] != "" && dstoffset_startend[1] != undefined){
			var dstoffset_end = trim(dstoffset_startend[1]);
			var dstoff_end = dstoffset_end.split(".");

			dstoff_end_m = parseInt(dstoff_end[0].substring(1));
			if(check_range(dstoff_end_m,1,12)){
				document.form.dst_end_m.value = dstoff_end_m;
			}

			if(dstoff_end[1] != "" && dstoff_end[1] != undefined){
				dstoff_end_w = parseInt(dstoff_end[1]);
				if(check_range(dstoff_end_w,1,5)){
					document.form.dst_end_w.value = dstoff_end_w;
				}
			}

			if(dstoff_end[2] != "" && dstoff_end[2] != undefined){
				dstoff_end_d = parseInt(dstoff_end[2].split("/")[0]);
				if(check_range(dstoff_end_d,0,6)){
					document.form.dst_end_d.value = dstoff_end_d;
				}

				dstoff_end_h = parseInt(dstoff_end[2].split("/")[1]);
				if(check_range(dstoff_end_h,0,23)){
					document.form.dst_end_h.value = dstoff_end_h;
				}
			}
		}
	}
}

function check_range(obj, first, last){
	if(obj != "NaN" && first <= obj && obj <= last)
		return true;
	else
		return false;
}

function hide_https_lanport(_value){
	if(tmo_support){
		document.getElementById("https_lanport").style.display = "none";
		return false;
	}

	if(sw_mode == '1' || sw_mode == '2' || sw_mode == '3'){
		var https_lanport_num = "<% nvram_get("https_lanport"); %>";
		document.getElementById("https_lanport").style.display = (_value == "0") ? "none" : "";
		document.form.https_lanport.disabled = (_value == "0") ? true : false;
		document.form.https_lanport.value = "<% nvram_get("https_lanport"); %>";
		document.getElementById("https_access_page").innerHTML = "<#https_access_url#> ";
		document.getElementById("https_access_page").innerHTML += "<a href=\"https://"+theUrl+":"+https_lanport_num+"\" target=\"_blank\" style=\"color:#FC0;text-decoration: underline; font-family:Lucida Console;\">http<span>s</span>://"+theUrl+"<span>:"+https_lanport_num+"</span></a>";
		document.getElementById("https_access_page").style.display = (_value == "0") ? "none" : "";
	}
	else{
		document.getElementById("https_access_page").style.display = 'none';
	}


	if(le_enable != "1" && _value != "0"){
		$("#https_download_cert").css("display", "");
		if(orig_http_enable == "0"){
			$("#download_cert_btn").css("display", "none");
			$("#download_cert_desc").css("display", "");
		}
		else{
			$("#download_cert_btn").css("display", "");
			$("#download_cert_desc").css("display", "none");
		}
	}
	else{
		$("#https_download_cert").css("display", "none");
	}
}

function hide_https_crt(){
	var protos = document.form.http_enable.value;

	showhide("cert_details", (protos != "0" ? 1 : 0));

	if ((!letsencrypt_support) || (sw_mode != 1)) {
		showhide("https_crt_san", (protos != "0" ? 1 : 0));
		showhide("https_crt_gen", (protos != "0" ? 1 : 0));
		showhide("https_cert", (protos != "0" ? 1 : 0));
		showhide("cert_manage_link", 0);
	}
}

// show clientlist
function show_http_clientlist(){
	var code = "";
	code +='<table width="100%" border="1" cellspacing="0" cellpadding="4" align="center" class="list_table" id="http_clientlist_table">';
	if(restrict_rulelist_array.length == 0) {
		code +='<tr><td style="color:#FFCC00;"><#IPConnection_VSList_Norule#></td></tr>';
	}
	else {
		var transformNumToText = function(restrict_type) {
			var bit_text_array = ["", "<#System_WebUI#>", "<#System_SSH#>", "<#System_Telnet#>"];
			var type_text = "";
			for(var i = 1; restrict_type != 0 && i <= 4; i += 1) {
				if(restrict_type & 1) {
					if(type_text == "")
						type_text += bit_text_array[i];
					else 
						type_text += ", " + bit_text_array[i];
				}
				restrict_type = restrict_type >> 1;
			}
			return type_text;
		};
		for(var i = 0; i < restrict_rulelist_array.length; i += 1) {
			if(restrict_rulelist_array[i][0] == "1")
				code += '<tr style="color:#FFFFFF;">';
			else
				code += '<tr style="color:#A0A0A0;">';
			code += '<td width="10%">';
			if(restrict_rulelist_array[i][0] == "1")
				code += '<div class="check" onclick="control_rule_status(this);"><div style="width:16px;height:16px;margin: 3px auto" class="icon_check"></div></div>';
			else
				code += '<div class="cancel" style="" onclick="control_rule_status(this);"><div style="width:16px;height:16px;margin: 3px auto" class="icon_cancel"></div></div>';
			code += '</td>';
			code += '<td width="40%">';
			code += restrict_rulelist_array[i][1];
			code += "</td>";
			code += '<td width="40%">';
			code += transformNumToText(restrict_rulelist_array[i][2]);
			code += "</td>";
			code += '<td width="10%">';
			code += '<div class="remove" style="margin:0 auto" onclick="deleteRow(this);">';
			code += "</td>";
			code += '</tr>';
		}
	}
	code +='</table>';
	document.getElementById("http_clientlist_Block").innerHTML = code;
}

function check_Timefield_checkbox(){	// To check Date checkbox checked or not and control Time field disabled or not
	if( document.form.reboot_date_x_Sun.checked == true 
		|| document.form.reboot_date_x_Mon.checked == true 
		|| document.form.reboot_date_x_Tue.checked == true
		|| document.form.reboot_date_x_Wed.checked == true
		|| document.form.reboot_date_x_Thu.checked == true
		|| document.form.reboot_date_x_Fri.checked == true
		|| document.form.reboot_date_x_Sat.checked == true	){
			inputCtrl(document.form.reboot_time_x_hour,1);
			inputCtrl(document.form.reboot_time_x_min,1);
			document.form.reboot_schedule.disabled = false;
	}
	else{
			inputCtrl(document.form.reboot_time_x_hour,0);
			inputCtrl(document.form.reboot_time_x_min,0);
			document.form.reboot_schedule.disabled = true;
			document.getElementById('reboot_schedule_time_tr').style.display ="";
	}
}

function deleteRow(r){
	var i=r.parentNode.parentNode.rowIndex;
	document.getElementById('http_clientlist_table').deleteRow(i);

 	restrict_rulelist_array.splice(i,1);

	if(restrict_rulelist_array.length == 0)
		show_http_clientlist();
}

function addRow(obj, upper){
	if('<% nvram_get("http_client"); %>' != "1")
		document.form.http_client_radio[0].checked = true;
		
	//Viz check max-limit 
	var rule_num = restrict_rulelist_array.length;
	if(rule_num >= upper){
		alert("<#JS_itemlimit1#> " + upper + " <#JS_itemlimit2#>");
		return false;
	}
			
	if(obj.value == ""){
		alert("<#JS_fieldblank#>");
		obj.focus();
		obj.select();
		return false;
	}
	else if(validator.validIPForm(obj, 2) != true){
		return false;
	}
	var access_type_value = 0;
	$(".access_type").each(function() {
		if(this.checked)
			access_type_value += parseInt($(this).val());
	});	
	if(access_type_value == 0) {
		alert("<#JS_access_type#>");
		return false;
	}
	else{
		var newRuleArray = new Array();
		newRuleArray.push("1");
		newRuleArray.push(obj.value.trim());
		newRuleArray.push(access_type_value.toString());
		var newRuleArray_tmp = new Array();
		newRuleArray_tmp = newRuleArray.slice();
		newRuleArray_tmp.splice(0, 1);
		newRuleArray_tmp.splice(1, 1);
		if(restrict_rulelist_array.length > 0) {
			for(var i = 0; i < restrict_rulelist_array.length; i += 1) {
				var restrict_rulelist_array_tmp = restrict_rulelist_array[i].slice();
				restrict_rulelist_array_tmp.splice(0, 1);
				restrict_rulelist_array_tmp.splice(1, 1);
				if(newRuleArray_tmp.toString() == restrict_rulelist_array_tmp.toString()) { //compare IP
					alert("<#JS_duplicate#>");
					return false;
				}
			}
		}
		restrict_rulelist_array.push(newRuleArray);
		
		obj.value = "";
		$(".access_type").each(function() {
			if(this.checked)
				$(this).prop('checked', false);
		});	
		
		show_http_clientlist();
		
		if($("#selAll").hasClass("all_enable"))
			$("#selAll").removeClass("all_enable").addClass("all_disable");
	}	
}

function keyBoardListener(evt){
	var nbr = (window.evt)?event.keyCode:event.which;
	if(nbr == 13)
		addRow(document.form.http_client_ip_x_0, 4);
}

function setClientIP(ipaddr){
	document.form.http_client_ip_x_0.value = ipaddr;
	hideClients_Block();
}

function hideClients_Block(){
	document.getElementById("pull_arrow").src = "/images/arrow-down.gif";
	document.getElementById('ClientList_Block_PC').style.display='none';
}

function pullLANIPList(obj){
	var element = document.getElementById('ClientList_Block_PC');
	var isMenuopen = element.offsetWidth > 0 || element.offsetHeight > 0;
	if(isMenuopen == 0){
		obj.src = "/images/arrow-top.gif"
		element.style.display = 'block';
		document.form.http_client_ip_x_0.focus();
	}
	else
		hideClients_Block();
}
//Viz add 2012.02 LAN client ip } end 

function hideport(flag){
	document.getElementById("accessfromwan_port").style.display = (flag == 1) ? "" : "none";
	if(!HTTPS_support){
		document.getElementById("NSlookup_help_for_WAN_access").style.display = (flag == 1) ? "" : "none";
		var orig_str = document.getElementById("access_port_title").innerHTML;
		document.getElementById("access_port_title").innerHTML = orig_str.replace(/HTTPS/, "HTTP");
		document.getElementById("http_port").style.display = (flag == 1) ? "" : "none";
		document.form.misc_httpport_x.disabled = (flag == 1) ? false: true;
	}
	else{
		document.getElementById("WAN_access_hint").style.display = (flag == 1) ? "" : "none";
		document.getElementById("wan_access_url").style.display = (flag == 1) ? "" : "none";
		change_url(document.form.misc_httpsport_x.value, "https_wan");
		document.getElementById("https_port").style.display = (flag == 1) ? "" : "none";
		document.form.misc_httpsport_x.disabled = (flag == 1) ? false: true;
	}
}

var autoChange = false;
function enable_wan_access(flag){
	if(HTTPS_support){
		if(flag == 1){
			if(document.form.http_enable.value == "0"){
				document.form.http_enable.selectedIndex = 2;
				autoChange = true;
				hide_https_lanport(document.form.http_enable.value);
			}
			alert("Note that enabling WAN access to the web interface is strongly discouraged as it's a security risk.  Please consider using a VPN instead.");
		}
		else{
			var effectApps = [];
			if(app_support) effectApps.push("<#RemoteAccessHint_RouterApp#>");
			if(alexa_support) effectApps.push("<#RemoteAccessHint_AlexaIFTTT#>");

			var original_misc_http_x = httpApi.nvramGet(["misc_http_x"]).misc_http_x;
			var RemoteAccessHint = "<#RemoteAccessHint#>".replace("$Apps$", effectApps.join(", "));

			if(original_misc_http_x == '1' && effectApps.length != 0){
				if(!confirm(RemoteAccessHint)){
					document.form.misc_http_x[0].checked = true;
					hideport(1);
					enable_wan_access(1);			
					return false;
				}
			}

			if(autoChange){
				document.form.http_enable.selectedIndex = 0;
				autoChange = false;
				hide_https_lanport(document.form.http_enable.value);
			}
		}
	}
}

function check_wan_access(http_enable){
	if(sw_mode == "1" && http_enable == "0" && document.form.misc_http_x[0].checked == true){	//While Accesss from WAN enabled, not allow to set HTTP only
		alert("When \"Web Access from WAN\" is enabled, HTTPS must be enabled.");
		document.form.http_enable.selectedIndex = 2;
		hide_https_lanport(document.form.http_enable.value);
	}
}

function change_url(num, flag){
	if(flag == 'https_lan'){
		var https_lanport_num_new = num;
		document.getElementById("https_access_page").innerHTML = "<#https_access_url#> ";
		document.getElementById("https_access_page").innerHTML += "<a href=\"https://"+theUrl+":"+https_lanport_num_new+"\" target=\"_blank\" style=\"color:#FC0;text-decoration: underline; font-family:Lucida Console;\">http<span>s</span>://"+theUrl+"<span>:"+https_lanport_num_new+"</span></a>";
	}
	else if(flag == 'https_wan'){
		var https_wanport = num;
		var host_addr = "";
		if(check_ddns_status())
				host_addr = ddns_hostname_x_t;
		else
			host_addr = wan_ipaddr;

		document.getElementById("wan_access_url").innerHTML = "<#https_access_url#> ";
		document.getElementById("wan_access_url").innerHTML += "<a href=\"https://"+host_addr+":"+https_wanport+"\" target=\"_blank\" style=\"color:#FC0;text-decoration: underline; font-family:Lucida Console;\">http<span>s</span>://"+host_addr+"<span>:"+https_wanport+"</span></a>";		
	}
}

/* password item show or not */
function pass_checked(obj){
	switchType(obj, document.form.show_pass_1.checked, true);
}

function select_time_zone(){
		
	if(document.form.time_zone_select.value.search("DST") >= 0 || document.form.time_zone_select.value.search("TDT") >= 0){	//DST area
			document.form.time_zone_dst.value=1;
			document.getElementById("dst_changes_start").style.display="";
			document.getElementById("dst_changes_end").style.display="";
			document.getElementById("dst_start").style.display="";
			document.getElementById("dst_end").style.display="";
		
	}
	else{
			document.form.time_zone_dst.value=0;
			document.getElementById("dst_changes_start").style.display="none";
			document.getElementById("dst_changes_end").style.display="none";
			document.getElementById("dst_start").style.display="none";
			document.getElementById("dst_end").style.display="none";
	}

	var getTimeZoneOffset = function(tz){
		return tz_table[tz] ? tz_table[tz] : "M3.2.0/2,M11.1.0/2";
	}

	var dstOffset = getTimeZoneOffset(document.form.time_zone_select.value)
	parse_dstoffset(dstOffset);
}

function clean_scorebar(obj){
	if(obj.value == "")
		document.getElementById("scorebarBorder").style.display = "none";
}

function check_sshd_enable(obj_value){
	var state;

	if (obj_value != 0)
		state = "";
	else
		state = "none";

	document.getElementById("remote_forwarding_tr").style.display = state;
	document.getElementById("auth_keys_tr").style.display = state;
	document.getElementById("sshd_password_tr").style.display = state;
	document.getElementById("sshd_port_tr").style.display = state;
	document.getElementById('SSH_Port_Suggestion1').style.display = state;
}

/*function sshd_remote_access(obj_value){
	if(obj_value == 1){
		document.getElementById('remote_access_port_tr').style.display = "";
	}
	else{
		document.getElementById('remote_access_port_tr').style.display = "none";
	}
}*/

/*function sshd_forward(obj_value){
	if(obj_value == 1){
		document.getElementById('remote_forwarding_port_tr').style.display = "";
	}
	else{
		document.getElementById('remote_forwarding_port_tr').style.display = "none";
	}

}*/
function telnet_enable(flag){
	document.getElementById('SSH_Port_Suggestion2').style.display = (flag == 1) ? "":"none";
}

function display_spec_IP(flag){
	if(flag == 0){
		document.getElementById("http_client_table").style.display = "none";
		document.getElementById("http_clientlist_Block").style.display = "none";
	}
	else{
		document.getElementById("http_client_table").style.display = "";
		document.getElementById("http_clientlist_Block").style.display = "";
		setTimeout("showDropdownClientList('setClientIP', 'ip', 'all', 'ClientList_Block_PC', 'pull_arrow', 'online');", 1000);
	}
}


function hide_reboot_option(flag){
	document.getElementById("reboot_schedule_date_tr").style.display = (flag == 1) ? "" : "none";
	document.getElementById("reboot_schedule_time_tr").style.display = (flag == 1) ? "" : "none";
	if(flag==1)
		check_Timefield_checkbox();
}


function getrebootTimeRange(str, pos)
{
	if (pos == 0)
		return str.substring(7,9);
	else if (pos == 1)
		return str.substring(9,11);
}

function setrebootTimeRange(rd, rh, rm)
{
	return(rd.value+rh.value+rm.value);
}

function updateDateTime()
{
	if(document.form.reboot_schedule_enable_x[0].checked){
		document.form.reboot_schedule_enable.value = "1";
		document.form.reboot_schedule.disabled = false;
		document.form.reboot_schedule.value = setDateCheck(
		document.form.reboot_date_x_Sun,
		document.form.reboot_date_x_Mon,
		document.form.reboot_date_x_Tue,
		document.form.reboot_date_x_Wed,
		document.form.reboot_date_x_Thu,
		document.form.reboot_date_x_Fri,
		document.form.reboot_date_x_Sat);
		document.form.reboot_schedule.value = setrebootTimeRange(
		document.form.reboot_schedule,
		document.form.reboot_time_x_hour,
		document.form.reboot_time_x_min);
	}
	else
		document.form.reboot_schedule_enable.value = "0";
}

function paste_password(){
	document.form.show_pass_1.checked = true;
	pass_checked(document.form.http_passwd2);
	pass_checked(document.form.v_password2);
}
function control_rule_status(obj) {
	var $itemObj = $(obj);
	var $trObj = $itemObj.closest('tr');
	var row_idx = $trObj.index();
	if($itemObj.hasClass("cancel")) {
		$itemObj.removeClass("cancel").addClass("check");
		$itemObj.children().removeClass("icon_cancel").addClass("icon_check");
		restrict_rulelist_array[row_idx][0] = "1";
		$trObj.css({"color" : "#FFFFFF"});
	}
	else {
		$itemObj.removeClass("check").addClass("cancel");
		$itemObj.children().removeClass("icon_check").addClass("icon_cancel");
		restrict_rulelist_array[row_idx][0] = "0";
		$trObj.css({"color" : "#A0A0A0"});
	}

	if($("#selAll").hasClass("all_enable"))
		$("#selAll").removeClass("all_enable").addClass("all_disable");
}
function control_all_rule_status(obj) {
	var set_all_rule_status = function(stauts) {
		for(var i = 0; i < restrict_rulelist_array.length; i += 1) {
			restrict_rulelist_array[i][0] = stauts;
		}
	};

	var $itemObj = $(obj);
	if($itemObj.hasClass("all_enable")) {
		$itemObj.removeClass("all_enable").addClass("all_disable");
		set_all_rule_status("1");
	}
	else {
		$itemObj.removeClass("all_disable").addClass("all_enable");
		set_all_rule_status("0");
	}

	show_http_clientlist();
}
function change_hddSpinDown(obj_value) {
	if(obj_value == "0") {
		$("#usb_idle_timeout_tr").css("display", "none");
		$("#usb_idle_exclude_tr").css("display", "none");
	}
	else {
		$("#usb_idle_timeout_tr").css("display", "");
		$("#usb_idle_exclude_tr").css("display", "");
	}
}

function open_upload_window(){
	$("#upload_cert_window").fadeIn(300);
}

function hide_upload_window(){
	$("#upload_cert_window").fadeOut(300);
}

function get_cert_info(){
	$.ajax({
		url: '/ajax_certinfo.asp',
		dataType: 'script',
		error: function(xhr){
			setTimeout("get_cert_info();", 1000);
		},
		success: function(response){
			show_cert_details();
	   }
	});
}

function show_cert_details(){
	document.getElementById("SAN").innerHTML = httpd_cert_info.SAN;
	document.getElementById("issueTo").innerHTML = httpd_cert_info.issueTo;
	document.getElementById("issueBy").innerHTML = httpd_cert_info.issueBy;
	document.getElementById("expireOn").innerHTML = httpd_cert_info.expire;
}

function check_filename(){
	var key_file = document.upload_form.file_key.value;
	var cert_file = document.upload_form.file_cert.value;
	var key_subname = key_file.substring(key_file.lastIndexOf('.') + 1);
	var cert_subname = cert_file.substring(cert_file.lastIndexOf('.') + 1);

	if(key_subname != 'pem' && key_subname != 'key'){
		alert("Please select correct private key file.");
		document.upload_form.file_key.value = "";
		document.upload_form.file_key.focus();
		return false;
	}

	if(cert_subname != 'pem' && cert_subname != 'crt' && cert_subname != 'cer'){
		alert("Please select correct SSL certificate file.");
		document.upload_form.file_cert.value = "";
		document.upload_form.file_cert.focus();
		return false;
	}

	return true;
}

function upload_cert_key(){
	if(check_filename()){
		document.upload_form.submit();
		hide_upload_window();
		setTimeout("get_cert_info();", 3000);
		uploaded_cert = true;
	}
}

function warn_jffs_format(){
	var msg = "WARNING: Erasing the JFFS partition will also wipe out some configuration elements such as OpenVPN certificates";
	if (hnd_support)
		msg += ", and various router settings"
	msg += ".\n\nMake sure you are certain you wish to proceed with this operation.";
	alert(msg);
}

function show_network_monitoring(){
	var orig_dns_probe = httpApi.nvramGet(["dns_probe"]).dns_probe;
	var orig_wandog_enable = httpApi.nvramGet(["wandog_enable"]).wandog_enable;

	appendMonitorOption(document.form.dns_probe_chk);
	appendMonitorOption(document.form.wandog_enable_chk);
	setTimeout("showPingTargetList();", 500);
}

function appendMonitorOption(obj){
	if(obj.name == "wandog_enable_chk"){
		if(obj.checked){
			document.getElementById("ping_tr").style.display = "";
			inputCtrl(document.form.wandog_target, 1);
		}
		else{
			document.getElementById("ping_tr").style.display = "none";
			inputCtrl(document.form.wandog_target, 0);
		}
	}
	else if(obj.name == "dns_probe_chk"){
		if(obj.checked){
			document.getElementById("probe_host_tr").style.display = "";
			document.getElementById("probe_content_tr").style.display = "";
			inputCtrl(document.form.dns_probe_host, 1);
			inputCtrl(document.form.dns_probe_content, 1);
		}
		else{
			document.getElementById("probe_host_tr").style.display = "none";
			document.getElementById("probe_content_tr").style.display = "none";
			inputCtrl(document.form.dns_probe_host, 0);
			inputCtrl(document.form.dns_probe_content, 0);
		}
	}
}

var isPingListOpen = 0;
function showPingTargetList(){
	if(is_CN){
		var APPListArray = [
			["Baidu", "www.baidu.com"], ["QQ", "www.qq.com"], ["Taobao", "www.taobao.com"]
		];
	}
	else{
		var APPListArray = [
			["Google ", "www.google.com"], ["Facebook", "www.facebook.com"], ["Youtube", "www.youtube.com"], ["Yahoo", "www.yahoo.com"],
			["Baidu", "www.baidu.com"], ["Wikipedia", "www.wikipedia.org"], ["Windows Live", "www.live.com"], ["QQ", "www.qq.com"],
			["Amazon", "www.amazon.com"], ["Twitter", "www.twitter.com"], ["Taobao", "www.taobao.com"], ["Blogspot", "www.blogspot.com"],
			["Linkedin", "www.linkedin.com"], ["Sina", "www.sina.com"], ["eBay", "www.ebay.com"], ["MSN", "msn.com"], ["Bing", "www.bing.com"],
			["Яндекс", "www.yandex.ru"], ["WordPress", "www.wordpress.com"], ["ВКонтакте", "www.vk.com"]
		];
	}

	var code = "";
	for(var i = 0; i < APPListArray.length; i++){
		code += '<a><div onclick="setPingTarget(\''+APPListArray[i][1]+'\');"><strong>'+APPListArray[i][0]+'</strong></div></a>';
	}
	code +='<!--[if lte IE 6.5]><iframe class="hackiframe2"></iframe><![endif]-->';
	document.getElementById("TargetList_Block_PC").innerHTML = code;
}

function setPingTarget(ipaddr){
	document.form.wandog_target.value = ipaddr;
	hidePingTargetList();
}

function hidePingTargetList(){
	document.getElementById("ping_pull_arrow").src = "/images/arrow-down.gif";
	document.getElementById('TargetList_Block_PC').style.display='none';
	isPingListOpen = 0;
}

function pullPingTargetList(obj){
	if(isPingListOpen == 0){
		obj.src = "/images/arrow-top.gif"
		document.getElementById("TargetList_Block_PC").style.display = 'block';
		document.form.wandog_target.focus();
		isPingListOpen = 1;
	}
	else
		hidePingTargetList();
}

function reset_portconflict_hint(){
	if($("#sshd_port").hasClass("highlight"))
		$("#sshd_port").removeClass("highlight");
	if($("#https_lanport_input").hasClass("highlight"))
		$("#https_lanport_input").removeClass("highlight");
	$("#port_conflict_sshdport").hide();
	$("#port_conflict_httpslanport").hide();
}

function myisPortConflict(_val, service){
	var str = "(" + _val + ") <#portConflictHint#>: ";

/* Check services on System page - through form */
	if (service != "ssh" && _val == document.form.sshd_port.value) {
		str = str + "SSH.";
		return str;
	}
	else if (service != "http" && _val == document.form.http_lanport.value) {
		str = str + "HTTP LAN port.";
		return str;
	}
	else if (service != "https" && _val == document.form.https_lanport.value) {
		str = str + "HTTPS LAN port.";
		return str;
	}
/* Check services on other pages - through nvram */
	else if(_val == '<% nvram_get("dm_http_port"); %>'){
		str = str + "<#DM_title#>.";
		return str;
		}
	else if(_val == '<% nvram_get("webdav_http_port"); %>'){
		str = str + "Cloud Disk.";
		return str;
	}
	else if(_val == '<% nvram_get("webdav_https_port"); %>'){
		str = str + "Cloud Disk.";
		return str;
	}
	else if(_val == '<% nvram_get("vpn_server1_port"); %>'){
		str = str + "OpenVPN Server 1.";
		return str;
	}
	else if(_val == '<% nvram_get("vpn_server2_port"); %>'){
		str = str + "OpenVPN Server 2.";
		return str;
	}
	else
		return false;
}


function save_cert_key(){
	location.href = "cert.tar";
}

var NTPListArray = [
		["time01.syd.optusnet.com.au", "time01.syd.optusnet.com.au"], ["time01.mel.optusnet.com.au", "time01.mel.optusnet.com.au"], 
		["time02.mel.optusnet.com.au", "time02.mel.optusnet.com.au"], ["au.pool.ntp.org", "au.pool.ntp.org"],	["time.nist.gov", "time.nist.gov"],
		["pool.ntp.org","pool.ntp.org"]
	];

function showNTPList(){
	var code = "";
	for(var i = 0; i < NTPListArray.length; i++){
		code += '<a><div onmouseover="over_var=1;" onmouseout="over_var=0;" onclick="setNTP(\''+NTPListArray[i][1]+'\');"><strong>'+NTPListArray[i][0]+'</strong></div></a>';
	}
	code +='<!--[if lte IE 6.5]><iframe class="hackiframe2"></iframe><![endif]-->';	
	document.getElementById("NTPList_Block_PC").innerHTML = code;
}

function setNTP(ntp_url){
	document.form.ntp_server0.value = ntp_url;
	hideNTP_Block();
	over_var = 0;
}

var over_var = 0;
var isMenuopen = 0;
function hideNTP_Block(){
	document.getElementById("ntp_pull_arrow").src = "/images/arrow-down.gif";
	document.getElementById('NTPList_Block_PC').style.display='none';
	isMenuopen = 0;
}

function pullNTPList(obj){
	if(isMenuopen == 0){		
		obj.src = "/images/arrow-top.gif"
		document.getElementById("NTPList_Block_PC").style.display = 'block';		
		document.form.ntp_server0.focus();		
		isMenuopen = 1;
	}
	else
		hideNTP_Block();
}
</script>
</head>

<body onload="initial();" onunLoad="return unload_body();" class="bg">
<div id="TopBanner"></div>

<div id="Loading" class="popup_bg"></div>

<iframe name="hidden_frame" id="hidden_frame" src="" width="0" height="0" frameborder="0"></iframe>
<form method="post" name="form" id="ruleForm" action="/start_apply.htm" target="hidden_frame">
<input type="hidden" name="productid" value="<% nvram_get("productid"); %>">
<input type="hidden" name="current_page" value="Advanced_System_Content.asp">
<input type="hidden" name="next_page" value="Advanced_System_Content.asp">
<input type="hidden" name="modified" value="0">
<input type="hidden" name="flag" value="">
<input type="hidden" name="action_mode" value="apply">
<input type="hidden" name="action_wait" value="5">
<input type="hidden" name="action_script" value="restart_time;restart_httpd;restart_upnp">
<input type="hidden" name="first_time" value="">
<input type="hidden" name="preferred_lang" id="preferred_lang" value="<% nvram_get("preferred_lang"); %>">
<input type="hidden" name="firmver" value="<% nvram_get("firmver"); %>">
<input type="hidden" name="time_zone_dst" value="<% nvram_get("time_zone_dst"); %>">
<input type="hidden" name="time_zone" value="<% nvram_get("time_zone"); %>">
<input type="hidden" name="time_zone_dstoff" value="<% nvram_get("time_zone_dstoff"); %>">
<input type="hidden" name="http_passwd" value="" disabled>
<input type="hidden" name="http_client" value="<% nvram_get("http_client"); %>"><!--for old fw-->
<input type="hidden" name="http_clientlist" value="<% nvram_get("http_clientlist"); %>"><!--for old fw-->
<input type="hidden" name="enable_acc_restriction" value="<% nvram_get("enable_acc_restriction"); %>">
<input type="hidden" name="restrict_rulelist" value="<% nvram_get("restrict_rulelist"); %>">
<input type="hidden" name="btn_ez_mode" value="<% nvram_get("btn_ez_mode"); %>">
<input type="hidden" name="reboot_schedule" value="<% nvram_get("reboot_schedule"); %>" disabled>
<input type="hidden" name="reboot_schedule_enable" value="<% nvram_get("reboot_schedule_enable"); %>">
<input type="hidden" name="usb_idle_exclude" value="<% nvram_get("usb_idle_exclude"); %>">
<input type="hidden" name="shell_timeout" value="<% nvram_get("shell_timeout"); %>">
<input type="hidden" name="sw_mode" value="<% nvram_get("sw_mode"); %>">
<input type="hidden" name="ncb_enable" value="<% nvram_get("ncb_enable"); %>">
<input type="hidden" name="dns_probe" value="<% nvram_get("dns_probe"); %>">
<input type="hidden" name="wandog_enable" value="<% nvram_get("wandog_enable"); %>">

<table class="content" align="center" cellpadding="0" cellspacing="0">
  <tr>
	<td width="17">&nbsp;</td>
	
	<!--=====Beginning of Main Menu=====-->
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
<table width="760px" border="0" cellpadding="4" cellspacing="0" class="FormTitle" id="FormTitle">
	<tbody>
	<tr>
		<td bgcolor="#4D595D" valign="top">
			<div>&nbsp;</div>
			<div class="formfonttitle"><#menu5_6#> - <#menu5_6_2#></div>
			<div style="margin:10px 0 10px 5px;" class="splitLine"></div>
			<div class="formfontdesc"><#System_title#></div>

			<table width="100%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3"  class="FormTable">
			<thead>
				<tr>
					<td colspan="2"><#PASS_changepasswd#></td>
				</tr>
			</thead>
				<tr>
				  <th width="40%"><#Router_Login_Name#></th>
					<td>
						<div><input type="text" id="http_username" name="http_username" tabindex="1" autocomplete="off" style="height:25px;" class="input_18_table" maxlength="20" autocorrect="off" autocapitalize="off"><br/><span id="alert_msg1" style="color:#FC0;margin-left:8px;display:inline-block;"></span></div>
					</td>
				</tr>

				<tr>
					<th width="40%"><a class="hintstyle" href="javascript:void(0);" onClick="openHint(11,4)"><#PASS_new#></a></th>
					<td>
						<input type="password" autocomplete="new-password" name="http_passwd2" tabindex="2" onKeyPress="return validator.isString(this, event);" onkeyup="chkPass(this.value, 'http_passwd');" onpaste="setTimeout('paste_password();', 10)" class="input_18_table" maxlength="17" onBlur="clean_scorebar(this);" autocorrect="off" autocapitalize="off"/>
						&nbsp;&nbsp;
						<div id="scorebarBorder" style="margin-left:180px; margin-top:-25px; display:none;" title="<#LANHostConfig_x_Password_itemSecur#>">
							<div id="score"></div>
							<div id="scorebar">&nbsp;</div>
						</div>            
					</td>
				</tr>
				<tr>
					<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(11,4)"><#PASS_retype#></a></th>
					<td>
						<input type="password" autocomplete="new-password" name="v_password2" tabindex="3" onKeyPress="return validator.isString(this, event);" onpaste="setTimeout('paste_password();', 10)" class="input_18_table" maxlength="17" autocorrect="off" autocapitalize="off"/>
						<div style="margin:-25px 0px 5px 175px;"><input type="checkbox" name="show_pass_1" onclick="pass_checked(document.form.http_passwd2);pass_checked(document.form.v_password2);"><#QIS_show_pass#></div>
						<span id="alert_msg2" style="color:#FC0;margin-left:8px;display:inline-block;"></span>
					
					</td>
				</tr>
				<tr id="login_captcha_tr" style="display:none">
					<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(11,13)">Enable Login Captcha</a></th>
					<td>
						<input type="radio" value="1" name="captcha_enable" <% nvram_match("captcha_enable", "1", "checked"); %>><#checkbox_Yes#>
						<input type="radio" value="0" name="captcha_enable" <% nvram_match("captcha_enable", "0", "checked"); %>><#checkbox_No#>
					</td>
				</tr>
			</table>
			<table width="100%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3"  class="FormTable">
				<thead>
					<tr>
						<td colspan="2">Persistent JFFS2 partition</td>
					</tr>
				</thead>
				<tr>
					<th>Format JFFS partition at next boot</th>
					<td>
						<input type="radio" name="jffs2_format" value="1" onclick="warn_jffs_format();" <% nvram_match("jffs2_format", "1", "checked"); %>><#checkbox_Yes#>
						<input type="radio" name="jffs2_format" value="0" <% nvram_match("jffs2_format", "0", "checked"); %>><#checkbox_No#>
					</td>
				</tr>
				<tr>
					<th>Enable JFFS custom scripts and configs</th>
					<td>
						<input type="radio" name="jffs2_scripts" value="1" <% nvram_match("jffs2_scripts", "1", "checked"); %>><#checkbox_Yes#>
						<input type="radio" name="jffs2_scripts" value="0" <% nvram_match("jffs2_scripts", "0", "checked"); %>><#checkbox_No#>
					</td>
				</tr>
			</table>

			<table id="hdd_spindown_table" width="100%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3"  class="FormTable" style="margin-top:8px;display:none;">
				<thead>
					<tr>
					  <td colspan="2"><#USB_Setting#></td>
					</tr>
				</thead>

				<tr>
					<th width="40%"><a class="hintstyle" href="javascript:void(0);" onClick="openHint(11,11)"><#usb_HDD_Hibernation#></a></th>
					<td>
						<select name="usb_idle_enable" class="input_option" onchange="change_hddSpinDown(this.value);" disabled>
							<option value="0" <% nvram_match("usb_idle_enable", "0", "selected"); %>><#checkbox_No#></option>
							<option value="1" <% nvram_match("usb_idle_enable", "1", "selected"); %>><#checkbox_Yes#></option>
						</select>
					</td>
				</tr>
				<tr id="usb_idle_timeout_tr">
					<th width="40%"><#TimePeriod#></th>
					<td>
						<input type="text" class="input_6_table" maxlength="4" name="usb_idle_timeout" onKeyPress="return validator.isNumber(this,event);" value='<% nvram_get("usb_idle_timeout"); %>' autocorrect="off" autocapitalize="off" disabled><#Second#>
						<span>(<#Setting_factorydefault_value#> : 300) </span>
					</td>
				</tr>
				<tr id="usb_idle_exclude_tr">
					<th>Exclude the following drives from spinning down</th>
					<td>
						<input type="checkbox" name="usb_idle_exclude_a">sda</input>
						<input type="checkbox" name="usb_idle_exclude_b">sdb</input>
						<input type="checkbox" name="usb_idle_exclude_c">sdc</input>
						<input type="checkbox" name="usb_idle_exclude_d">sdd</input>
						<input type="checkbox" name="usb_idle_exclude_e">sde</input>
						<input type="checkbox" name="usb_idle_exclude_f">sdf</input>
						<input type="checkbox" name="usb_idle_exclude_g">sdg</input>
						<input type="checkbox" name="usb_idle_exclude_h">sdh</input>
						<input type="checkbox" name="usb_idle_exclude_i">sdi</input>
					</td>
				</tr>
				<tr id="reduce_usb3_tr" style="display:none">
					<th width="40%"><a class="hintstyle" href="javascript:void(0);" onclick="openHint(3, 29)"><#select_usb_mode#></a></th>
					<td>
						<select class="input_option" name="usb_usb3" onchange="enableUsbMode(this.value);">
							<option value="0" <% nvram_match("usb_usb3", "0", "selected"); %>>USB 2.0</option>
							<option value="1" <% nvram_match("usb_usb3", "1", "selected"); %>>USB 3.0</option>
						</select>
						<script>
							var needReboot = false;


							if (isSupport("usb3")) {
								$("#reduce_usb3_tr").show();
							}

							function enableUsbMode(v){
								httpApi.nvramGetAsync({
									data: ["usb_usb3"],
									success: function(nvram){
										needReboot = (nvram.usb_usb3 != v);
									}
								})
							}
						</script>
					</td>
				</tr>
			</table>

			<table width="100%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3"  class="FormTable" style="margin-top:8px;">
				<thead>
					<tr>
					  <td colspan="2"><#t2BC#></td>
					</tr>
				</thead>
				<tr id="ntpd_server_tr">
					<th>Enable local NTP server</th>
					<td>
						<input type="radio" name="ntpd_enable" value="1" onclick="showhide('ntpd_redir_tr', 1);" <% nvram_match_x("","ntpd_enable","1", "checked"); %> ><#checkbox_Yes#>
						<input type="radio" name="ntpd_enable" value="0" onclick="showhide('ntpd_redir_tr', 0);" <% nvram_match_x("","ntpd_enable","0", "checked"); %> ><#checkbox_No#>
					</td>
				</tr>
				<tr id="ntpd_redir_tr">
					<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(50,29)">Intercept NTP client requests</a></th>
					<td>
						<input type="radio" name="ntpd_server_redir" value="1" <% nvram_match_x("","ntpd_server_redir","1", "checked"); %> ><#checkbox_Yes#>
						<input type="radio" name="ntpd_server_redir" value="0" <% nvram_match_x("","ntpd_server_redir","0", "checked"); %> ><#checkbox_No#>
					</td>
				<tr>
					<th><a class="hintstyle"  href="javascript:void(0);" onClick="openHint(11,2)"><#LANHostConfig_x_TimeZone_itemname#></a></th>
					<td>
						<select name="time_zone_select" class="input_option" onchange="select_time_zone();autofill_dst();"></select>
						<div>
							<span id="timezone_hint" style="display:none;"></span>
						</div>
					</td>
				</tr>
				<tr id="dst_changes_start" style="display:none;">
					<th><a class="hintstyle"  href="javascript:void(0);" onClick="openHint(11,7)"><#LANHostConfig_x_TimeZone_DSTStart#></a></th>
					<td>
								<div id="dst_start" style="color:white;display:none;">
									<div>
										<select name="dst_start_m" class="input_option"></select>&nbsp;<#month#> &nbsp;
										<select name="dst_start_w" class="input_option"></select>&nbsp;
										<select name="dst_start_d" class="input_option"></select>&nbsp;<#diskUtility_week#> & <#Day#> &nbsp;
										<select name="dst_start_h" class="input_option"></select>&nbsp;<#Hour#> &nbsp;
										<script>
											for(var i = 1; i < dst_month.length; i++){
												add_option(document.form.dst_start_m, dst_month[i], i, 0);
											}
											for(var i = 1; i < dst_week.length; i++){
												add_option(document.form.dst_start_w, dst_week[i], i, 0);
											}	
											for(var i = 0; i < dst_day.length; i++){
												add_option(document.form.dst_start_d, dst_day[i], i, 0);
											}
											for(var i = 0; i < dst_hour.length; i++){
												add_option(document.form.dst_start_h, dst_hour[i], i, 0);
											}
										</script>
									</div>
								</div>
					</td>
				</tr>
				<tr id="dst_changes_end" style="display:none;">
					<th><a class="hintstyle"  href="javascript:void(0);" onClick="openHint(11,8)"><#LANHostConfig_x_TimeZone_DSTEnd#></a></th>
					<td>
								<div id="dst_end" style="color:white;display:none;">
									<div>
										<select name="dst_end_m" class="input_option"></select>&nbsp;<#month#> &nbsp;
										<select name="dst_end_w" class="input_option"></select>&nbsp;
										<select name="dst_end_d" class="input_option"></select>&nbsp;<#diskUtility_week#> & <#Day#> &nbsp;
										<select name="dst_end_h" class="input_option"></select>&nbsp;<#Hour#> &nbsp;
										<script>
											for(var i = 1; i < dst_month.length; i++){
												add_option(document.form.dst_end_m, dst_month[i], i, 0);
											}
											for(var i = 1; i < dst_week.length; i++){
												add_option(document.form.dst_end_w, dst_week[i], i, 0);
											}
											for(var i = 0; i < dst_day.length; i++){
												add_option(document.form.dst_end_d, dst_day[i], i, 0);
											}
											for(var i = 0; i < dst_hour.length; i++){
												add_option(document.form.dst_end_h, dst_hour[i], i, 0);
											}
										</script>
									</div>
								</div>
					</td>
				</tr>
				<tr>
					<th><a class="hintstyle"  href="javascript:void(0);" onClick="openHint(11,3)"><#LANHostConfig_x_NTPServer_itemname#></a></th>
					<td>
						<input type="text" maxlength="255" class="input_32_table" name="ntp_server0" value="<% nvram_get("ntp_server0"); %>" onKeyPress="return validator.isString(this, event);" autocorrect="off" autocapitalize="off">
						<img id="ntp_pull_arrow" height="14px;" src="/images/arrow-down.gif" style="position:absolute;*margin-left:-3px;*margin-top:1px;display:none;" onclick="pullNTPList(this);" title="<#LANHostConfig_x_NTPServer_itemname#>" onmouseover="over_var=1;" onmouseout="over_var=0;">
						<div id="NTPList_Block_PC" class="NTPList_Block_PC"></div>
						<br>

						<a href="javascript:openLink('x_NTPServer1')"  name="x_NTPServer1_link" style=" margin-left:5px; text-decoration: underline;"><#LANHostConfig_x_NTPServer1_linkname#></a>
						<div id="svc_hint_div" style="display:none;">
							<span style="color:#FFCC00;"><#General_x_SystemTime_syncNTP#></span>
							<a id="ntp_faq" href="" target="_blank" style="margin-left:5px; color: #FFCC00; text-decoration: underline;">FAQ</a>
						</div>
					</td>
				</tr>
				<tr>
					<th>Secondary NTP Server</th>
					<td>
						<input type="text" maxlength="255" class="input_32_table" name="ntp_server1" value="<% nvram_get("ntp_server1"); %>" onKeyPress="return validator.isString(this, event);" autocorrect="off" autocapitalize="off">
					</td>
				</tr>
				<tr id="network_monitor_tr">
					<th><#Network_Monitoring#></th>
					<td>
						<input type="checkbox" name="dns_probe_chk" value="" <% nvram_match("dns_probe", "1", "checked"); %> onClick="appendMonitorOption(this);"><div style="display: inline-block; vertical-align: middle; margin-bottom: 2px;" ><#DNS_Query#></div>
						<input type="checkbox" name="wandog_enable_chk" value="" <% nvram_match("wandog_enable", "1", "checked"); %>  onClick="appendMonitorOption(this);"><div style="display: inline-block; vertical-align: middle; margin-bottom: 2px;"><#Ping#></div>
					</td>
				</tr>
				<tr id="probe_host_tr" style="display: none;">
					<th><#Resolved_Target#></th>
					<td>
							<input type="text" class="input_32_table" name="dns_probe_host" maxlength="255" autocorrect="off" autocapitalize="off" value="<% nvram_get("dns_probe_host"); %>">
					</td>
				</tr>
				<tr id="probe_content_tr" style="display: none;">
					<th><#Respond_IP#></th>
					<td>
							<input type="text" class="input_32_table" name="dns_probe_content" maxlength="1024" autocorrect="off" autocapitalize="off" value="<% nvram_get("dns_probe_content"); %>">
					</td>
				</tr>
				<tr id="ping_tr" style="display: none;">
					<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(26,2);"><#Ping_Target#></a></th>
					<td>
							<input type="text" class="input_32_table" name="wandog_target" maxlength="100" value="<% nvram_get("wandog_target"); %>" placeholder="ex: www.google.com" autocorrect="off" autocapitalize="off">
							<img id="ping_pull_arrow" class="pull_arrow" height="14px;" src="/images/arrow-down.gif" style="position:absolute;*margin-left:-3px;*margin-top:1px;" onclick="pullPingTargetList(this);" title="<#select_network_host#>">
							<div id="TargetList_Block_PC" name="TargetList_Block_PC" class="clientlist_dropdown" style="margin-left: 2px; width: 348px;display: none;"></div>
					</td>
				</tr>
				<tr>
					<th><#System_AutoLogout#></th>
					<td>
						<input type="text" class="input_3_table" maxlength="3" name="http_autologout" value='<% nvram_get("http_autologout"); %>' onKeyPress="return validator.isNumber(this,event);" autocorrect="off" autocapitalize="off"> <#Minute#>
						<span>(<#zero_disable#>)</span>
					</td>
				</tr>
				<tr id="nat_redirect_enable_tr">
					<th align="right"><a class="hintstyle" href="javascript:void(0);" onClick="openHint(11,6);"><#Enable_redirect_notice#></a></th>
					<td>
						<input type="radio" name="nat_redirect_enable" value="1" <% nvram_match_x("","nat_redirect_enable","1", "checked"); %> ><#checkbox_Yes#>
						<input type="radio" name="nat_redirect_enable" value="0" <% nvram_match_x("","nat_redirect_enable","0", "checked"); %> ><#checkbox_No#>
					</td>
				</tr>
				<tr>
					<th>Redirect webui access to router.asus.com</th>
					<td>
						<input type="radio" name="http_dut_redir" value="1" <% nvram_match_x("","http_dut_redir","1", "checked"); %> ><#checkbox_Yes#>
						<input type="radio" name="http_dut_redir" value="0" <% nvram_match_x("","http_dut_redir","0", "checked"); %> ><#checkbox_No#>
					</td>
				</tr>

				<tr id="btn_ez_radiotoggle_tr" style="display: none;">
					<th><#WPS_btn_behavior#></th>
					<td>
						<label for="turn_WPS" id="btn_ez_WPS">
							<input type="radio" name="btn_ez_radiotoggle" id="turn_WPS" class="input" value="0"><#WPS_btn_actWPS#>
						</label>
						<label for="turn_WiFi" id="btn_ez_WiFi" style="display:none;">
							<input type="radio" name="btn_ez_radiotoggle" id="turn_WiFi" class="input" value="1"><#WPS_btn_toggle#>
						</label>
						<label for="turn_LED" id="btn_ez_LED" style="display:none;">
							<input type="radio" name="btn_ez_radiotoggle" id="turn_LED" class="input" value="0"><#LED_switch#>
						</label>					
					</td>
				</tr>
				<tr>
					<th>Disable LEDs</th>
					<td>
						<input type="radio" name="led_disable" value="1" <% nvram_match_x("", "led_disable", "1", "checked"); %>><#checkbox_Yes#>
						<input type="radio" name="led_disable" value="0" <% nvram_match_x("", "led_disable", "0", "checked"); %>><#checkbox_No#>
					</td>
				</tr>
				<tr id="pwrsave_tr">
					<th align="right"><#usb_Power_Save_Mode#></th>
					<td>
						<select name="pwrsave_mode" class="input_option">
							<option value="0" <% nvram_match("pwrsave_mode", "0","selected"); %> ><#usb_Performance#></option>
							<option value="1" <% nvram_match("pwrsave_mode", "1","selected"); %> ><#Auto#></option>
							<option value="2" <% nvram_match("pwrsave_mode", "2","selected"); %> ><#usb_Power_Save#></option>
						</select>
					</td>
				</tr>
				<tr id="reboot_schedule_enable_tr">
					<th><#Enable_reboot_scheduler#></th>
					<td>
						<input type="radio" value="1" name="reboot_schedule_enable_x" onClick="hide_reboot_option(1);" <% nvram_match_x("LANHostConfig","reboot_schedule_enable", "1", "checked"); %>><#checkbox_Yes#>
						<input type="radio" value="0" name="reboot_schedule_enable_x" onClick="hide_reboot_option(0);" <% nvram_match_x("LANHostConfig","reboot_schedule_enable", "0", "checked"); %>><#checkbox_No#>
					</td>
				</tr>
				<tr id="reboot_schedule_date_tr">
					<th><#Reboot_Date#></th>
					<td>
						<input type="checkbox" name="reboot_date_x_Sun" class="input" onclick="check_Timefield_checkbox();"><#date_Sun_itemdesc#>
						<input type="checkbox" name="reboot_date_x_Mon" class="input" onclick="check_Timefield_checkbox();"><#date_Mon_itemdesc#>
						<input type="checkbox" name="reboot_date_x_Tue" class="input" onclick="check_Timefield_checkbox();"><#date_Tue_itemdesc#>
						<input type="checkbox" name="reboot_date_x_Wed" class="input" onclick="check_Timefield_checkbox();"><#date_Wed_itemdesc#>
						<input type="checkbox" name="reboot_date_x_Thu" class="input" onclick="check_Timefield_checkbox();"><#date_Thu_itemdesc#>
						<input type="checkbox" name="reboot_date_x_Fri" class="input" onclick="check_Timefield_checkbox();"><#date_Fri_itemdesc#>
						<input type="checkbox" name="reboot_date_x_Sat" class="input" onclick="check_Timefield_checkbox();"><#date_Sat_itemdesc#>
					</td>
				</tr>
				<tr id="reboot_schedule_time_tr">
					<th><#Reboot_Time#></th>
					<td>
						<input type="text" maxlength="2" class="input_3_table" name="reboot_time_x_hour" onKeyPress="return validator.isNumber(this,event);" onblur="validator.timeRange(this, 0);" autocorrect="off" autocapitalize="off"> :
						<input type="text" maxlength="2" class="input_3_table" name="reboot_time_x_min" onKeyPress="return validator.isNumber(this,event);" onblur="validator.timeRange(this, 1);" autocorrect="off" autocapitalize="off">
					</td>
				</tr>
				<tr id="ncb_enable_option_tr" style="display:none">
					<th><#Enable_ncb_notice#></th>
					<td>
						<select name="ncb_enable_option" class="input_option">
							<option value="0" <% nvram_match("ncb_enable", "0", "selected"); %>><#Enable_ncb_Non_Block#></option>
							<option value="1" <% nvram_match("ncb_enable", "1", "selected"); %>><#Enable_ncb_Limited_Block#></option>
							<option value="2" <% nvram_match("ncb_enable", "2", "selected"); %>><#Enable_ncb_All_Block#></option>
						</select>
					</td>
				</tr>

				<tr id="sw_mode_radio_tr" style="display:none">
					<th><#OP_AP_item#></th>
					<td>
						<input type="radio" name="sw_mode_radio" value="1" <% nvram_match_x("","sw_mode","3", "checked"); %> ><#checkbox_Yes#>
						<input type="radio" name="sw_mode_radio" value="0" <% nvram_match_x("","sw_mode","1", "checked"); %> ><#checkbox_No#>
					</td>
				</tr>
			</table>

			<table id="telnetd_sshd_table" width="100%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3"  class="FormTable" style="margin-top:8px;">
				<thead>
					<tr>
					  <td colspan="2"><#qis_service#></td>
					</tr>
				</thead>
				<tr id="telnet_tr" style="display:none;">
					<th><#Enable_Telnet#></th>
					<td>
						<input type="radio" name="telnetd_enable" value="1" onchange="telnet_enable(this.value);" <% nvram_match_x("LANHostConfig", "telnetd_enable", "1", "checked"); %>><#checkbox_Yes#>
						<input type="radio" name="telnetd_enable" value="0" onchange="telnet_enable(this.value);" <% nvram_match_x("LANHostConfig", "telnetd_enable", "0", "checked"); %>><#checkbox_No#>
						<div style="color: #FFCC00;display:none;" id="SSH_Port_Suggestion2">* <#SSH_Port_Suggestion2#></div>
					</td>
				</tr>
				<tr id="sshd_enable_tr">
					<th width="40%"><#Enable_SSH#></th>
					<td>
						<select name="sshd_enable" class="input_option" onchange="check_sshd_enable(this.value);">
							<option value="0" <% nvram_match("sshd_enable", "0", "selected"); %>><#checkbox_No#></option>
							<option value="2" <% nvram_match("sshd_enable", "2", "selected"); %>>LAN only</option>
							<option value="1" <% nvram_match("sshd_enable", "1", "selected"); %>>LAN & WAN</option>
						</select>
					</td>
				</tr>
				<tr id="remote_forwarding_tr">
					<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(50,10);">Allow SSH Port Forwarding</a></th>
					<td>
						<input type="radio" name="sshd_forwarding" value="1" <% nvram_match("sshd_forwarding", "1", "checked"); %>><#checkbox_Yes#>
						<input type="radio" name="sshd_forwarding" value="0" <% nvram_match("sshd_forwarding", "0", "checked"); %>><#checkbox_No#>
					</td>
				</tr>
				<tr id="sshd_port_tr">
					<th><#Port_SSH#></th>
					<td>
						<input type="text" class="input_6_table" maxlength="5" id="sshd_port" name="sshd_port" onKeyPress="return validator.isNumber(this,event);" autocorrect="off" autocapitalize="off" value='<% nvram_get("sshd_port"); %>' onkeydown="reset_portconflict_hint();">
						<span id="port_conflict_sshdport" style="color: #e68282; display: none;">Port Conflict</span>
						<div style="color: #FFCC00;display:none;" id="SSH_Port_Suggestion1">* Using a different port than the default port 22 is recommended to avoid port scan attacks.</div>
					</td>
				</tr>
				<tr id="sshd_password_tr">
					<th><#Allow_PWLogin#></th>
					<td>
						<input type="radio" name="sshd_pass" value="1" <% nvram_match("sshd_pass", "1", "checked"); %>><#checkbox_Yes#>
						<input type="radio" name="sshd_pass" value="0" <% nvram_match("sshd_pass", "0", "checked"); %>><#checkbox_No#>
					</td>
				</tr>
				<tr id="auth_keys_tr">
					<th><#Authorized_Keys#></th>
					<td>
						<textarea rows="8" class="textarea_ssh_table" style="width:98%; overflow:auto; word-break:break-all;" name="sshd_authkeys" style="width:95%;" spellcheck="false" maxlength="2999"><% nvram_clean_get("sshd_authkeys"); %></textarea>
						<span id="ssh_alert_msg"></span>
					</td>
				</tr>
				<tr id="plc_sleep_tr" style="display:none;">
					<th align="right"><a class="hintstyle" href="javascript:void(0);" onClick="openHint(11,12);">Enable PLC sleep automatically<!--untranslated--></a></th>
					<td>
						<input type="radio" name="plc_sleep_enabled" value="1" <% nvram_match_x("","plc_sleep_enabled","1", "checked"); %> ><#checkbox_Yes#>
						<input type="radio" name="plc_sleep_enabled" value="0" <% nvram_match_x("","plc_sleep_enabled","0", "checked"); %> ><#checkbox_No#>
					</td>
				</tr>
				<tr>
					<th width="40%"><#FreeWiFi_Idle#></th>
					<td>
						<input type="text" class="input_3_table" maxlength="3" name="shell_timeout_x" value="" onKeyPress="return validator.isNumber(this,event);" autocorrect="off" autocapitalize="off"> <#Minute#>
						<span>(<#zero_disable#>)</span>
					</td>
				</tr>

				<tr id="https_download_cert" style="display: none;">
					<th><#Local_access_certificate_download#></th>
					<td>
						<input id="download_cert_btn" class="button_gen" onclick="save_cert_key();" type="button" value="<#btn_Export#>" />
						<span id="download_cert_desc"><#Local_access_certificate_desc#></span><a href="https://www.asus.com/support/FAQ/1034294" style="font-family:Lucida Console;text-decoration:underline;color:#FFCC00; margin-left: 5px;" target="_blank">FAQ</a>
					</td>
				</tr>
			</table>

			<table id ="http_auth_table" width="100%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3"  class="FormTable" style="margin-top:8px;">
				<thead>
					<tr>
					  <td colspan="2"><#Local_access_config#></td>
					</tr>
				</thead>
				<tr id="https_tr">
					<th><#WLANConfig11b_AuthenticationMethod_itemname#></th>
					<td>
						<select name="http_enable" class="input_option" onchange="hide_https_lanport(this.value);check_wan_access(this.value);">
							<option value="0" <% nvram_match("http_enable", "0", "selected"); %>>HTTP</option>
							<option value="1" <% nvram_match("http_enable", "1", "selected"); %>>HTTPS</option>
							<option value="2" <% nvram_match("http_enable", "2", "selected"); %>>BOTH</option>
						</select>
					</td>
				</tr>
				<tr id="http_lanport">
					<th>HTTP LAN port</th>
					<td>
						<input type="text" maxlength="5" class="input_6_table" name="http_lanport" value="<% nvram_get("http_lanport"); %>" onKeyPress="return validator.isNumber(this,event);" onBlur="change_url(this.value, 'http_lan');" autocorrect="off" autocapitalize="off">
						<span id="http_access_page"></span>
					</td>
				</tr>
				<tr id="https_lanport">
					<th><#System_HTTPS_LAN_Port#></th>
					<td>
						<input type="text" maxlength="5" class="input_6_table" id="https_lanport_input" name="https_lanport" value="<% nvram_get("https_lanport"); %>" onKeyPress="return validator.isNumber(this,event);" onBlur="change_url(this.value, 'https_lan');" autocorrect="off" autocapitalize="off" onkeydown="reset_portconflict_hint();" disabled>
						<span id="port_conflict_httpslanport" style="color: #e68282; display: none;">Port Conflict</span>
						<div id="https_access_page" style="color: #FFCC00;"></div>
						<div style="color: #FFCC00; display: none;">* <#HttpsLanport_Hint#></div>
					</td>
				</tr>
                                <tr id="https_crt_gen" style="display:none;">
                                        <th>Generate a new certificate</th>
                                        <td>
						<input type="radio" name="https_crt_gen" class="input" value="1" onClick="hide_https_crt();" <% nvram_match_x("", "https_crt_gen", "1", "checked"); %>><#checkbox_Yes#>
						<input type="radio" name="https_crt_gen" class="input" value="0" onClick="hide_https_crt();" <% nvram_match_x("", "https_crt_gen", "0", "checked"); %>><#checkbox_No#>
                                        </td>
                                </tr>
				<tr id="https_crt_san" style="display:none;">
					<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(50,22)">Additional Certificate SANs</a></th>
					<td>
						<input type="text" name="https_crt_cn" value="<% nvram_get("https_crt_cn"); %>" autocomplete="off" class="input_32_table" maxlength="64" autocorrect="off" autocapitalize="off">
					</td>
				</tr>

				<tr id="https_cert" style="display:none;">
					<th>Provide your own certificate</th>
					<td>
						<input type="radio" value="2" name="le_enable" <% nvram_match("le_enable", "2", "checked"); %>>Import or Persistent Auto-generated
						<input type="radio" value="0" name="le_enable" <% nvram_match("le_enable", "0", "checked"); %>>Non-persistent auto-generated
						<div id="cert_act" style="margin-top: 5px;"><div style="display:table-cell"><input class="button_gen" onclick="open_upload_window();" type="button" value="<#CTL_upload#>"/><img id="loadingicon" style="margin-left:5px;display:none;" src="/images/InternetScan.gif"></div></div>
					</td>
				</tr>
				<tr id="cert_details" style="display:none;">
					<th>Installed Server Certificate</th>
					<td>
						<div style="display:table-row;">
							<div style="display:table-cell;white-space: nowrap;">Issued to :</div>
							<div id="issueTo" style="display:table-cell; padding-left:10px;"></div>
						</div>
						<div style="display:table-row;">
							<div style="display:table-cell;white-space: nowrap">SAN :</div>
							<div id="SAN" style="display:table-cell; padding-left:10px;"></div>
						</div>
						<div style="display:table-row;">
							<div style="display:table-cell;white-space: nowrap">Issued by :</div>
							<div id="issueBy" style="display:table-cell; padding-left:10px;"></div>
						</div>
						<div style="display:table-row;">
							<div style="display:table-cell;white-space: nowrap">Expires on :</div>
							<div id="expireOn" style="display:table-cell; padding-left:10px;"></div>
						</div>
						<div id="cert_manage_link" style="padding-top:10px;"><span>Click <a style="color:#FC0;text-decoration: underline;" href="Advanced_ASUSDDNS_Content.asp">here</a> to manage.</span></div>
					</td>
				</tr>

			</table>

			<table width="100%" border="1" align="center" cellpadding="4" cellspacing="0" bordercolor="#6b8fa3"  class="FormTable" style="margin-top:8px;">
				<thead>
					<tr>
					  <td colspan="2"><#Remote_access_config#></td>
					</tr>
				</thead>
				<tr id="misc_http_x_tr">
					<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(8,2);"><#FirewallConfig_x_WanWebEnable_itemname#></a></th>
					<td>
						<input type="radio" value="1" name="misc_http_x" onClick="hideport(1);enable_wan_access(1);" <% nvram_match("misc_http_x", "1", "checked"); %>><#checkbox_Yes#>
						<input type="radio" value="0" name="misc_http_x" onClick="hideport(0);enable_wan_access(0);" <% nvram_match("misc_http_x", "0", "checked"); %>><#checkbox_No#><br>
						<span class="formfontdesc" id="WAN_access_hint" style="color:#FFCC00; display:none;"><#FirewallConfig_x_WanWebEnable_HTTPS_only#> 
							<a id="faq" href="" target="_blank" style="margin-left: 5px; color:#FFCC00; text-decoration: underline;">FAQ</a>
						</span>
						<div class="formfontdesc" id="NSlookup_help_for_WAN_access" style="color:#FFCC00; display:none;"><#NSlookup_help#></div>
					</td>
				</tr>
				<tr id="accessfromwan_port">
					<th align="right"><a id="access_port_title" class="hintstyle" href="javascript:void(0);" onClick="openHint(8,3);">HTTPS <#FirewallConfig_x_WanWebPort_itemname#></a></th>
					<td>
						<span style="margin-left:5px; display:none;" id="http_port"><input type="text" maxlength="5" name="misc_httpport_x" class="input_6_table" value="<% nvram_get("misc_httpport_x"); %>" onKeyPress="return validator.isNumber(this,event);" autocorrect="off" autocapitalize="off" disabled/>&nbsp;&nbsp;</span>
						<span style="margin-left:5px; display:none;" id="https_port"><input type="text" maxlength="5" name="misc_httpsport_x" class="input_6_table" value="<% nvram_get("misc_httpsport_x"); %>" onKeyPress="return validator.isNumber(this,event);" onBlur="change_url(this.value, 'https_wan');" autocorrect="off" autocapitalize="off" disabled/></span>
						<span id="wan_access_url"></span>
					</td>
				</tr>
				<tr>
					<th><a class="hintstyle" href="javascript:void(0);" onClick="openHint(11,10);"><#System_login_specified_IP#></a></th>
					<td>
						<input type="radio" name="http_client_radio" value="1" onclick="display_spec_IP(1);" <% nvram_match_x("", "http_client", "1", "checked"); %>><#checkbox_Yes#>
						<input type="radio" name="http_client_radio" value="0" onclick="display_spec_IP(0);" <% nvram_match_x("", "http_client", "0", "checked"); %>><#checkbox_No#>
					</td>
				</tr>
			</table>

			<table width="100%" border="1" align="center" cellpadding="4" cellspacing="0" class="FormTable_table" id="http_client_table">
				<thead>
					<tr>
						<td colspan="4"><#System_login_specified_Iplist#>&nbsp;(<#List_limit#>&nbsp;4)</td>
					</tr>
				</thead>
				
				<tr>
					<th width="10%"><div id="selAll" class="all_disable" style="margin: auto;width:40px;" onclick="control_all_rule_status(this);"><#All#></div></th>
					<th width="40%"><a class="hintstyle" href="javascript:void(0);" onClick="openHint(6,1);"><#RouterConfig_GWStaticIP_itemname#></a></th>
					<th width="40%"><#Access_Type#></th>
					<th width="10%"><#list_add_delete#></th>
				</tr>

				<tr>
					<!-- client info -->
					<td width="10%">-</td>
					<td width="40%">
						<input type="text" class="input_25_table" maxlength="18" name="http_client_ip_x_0"  onKeyPress="" onClick="hideClients_Block();" autocorrect="off" autocapitalize="off">
						<img id="pull_arrow" height="14px;" src="/images/arrow-down.gif" style="position:absolute;*margin-left:-3px;*margin-top:1px;" onclick="pullLANIPList(this);" title="<#select_client#>">	
						<div id="ClientList_Block_PC" class="clientlist_dropdown" style="margin-left:27px;width:235px;"></div>	
					</td>
					<td width="40%">
						<input type="checkbox" name="access_webui" class="input access_type" value="1"><#System_WebUI#>
						<input type="checkbox" name="access_ssh" class="input access_type" value="2"><#System_SSH#>
						<!-- input type="checkbox" name="access_telnet" class="input access_type" value="4"><#System_Telnet#> -->
					</td>
					<td width="10%">
						<div id="add_delete" class="add_enable" style="margin:0 auto" onclick="addRow(document.form.http_client_ip_x_0, 4);"></div>
					</td>
				</tr>
			</table>
			<div id="http_clientlist_Block"></div>
			<div class="apply_gen">
				<input name="button" type="button" class="button_gen" onclick="applyRule();" value="<#CTL_apply#>"/>
			</div>
		</td>
	</tr>
</tbody>

</table></td>
</form>
        </tr>
    </table>
		<!--===================================Ending of Main Content===========================================-->		
	</td>
		
    <td width="10" align="center" valign="top">&nbsp;</td>
	</tr>
</table>

<div id="footer"></div>
<form method="post" name="upload_form" action="upload_cert_key.cgi" target="hidden_frame" enctype="multipart/form-data">
<input type="hidden" name="action_mode" value="">
<input type="hidden" name="action_script" value="">
<input type="hidden" name="action_wait" value="">
<div id="upload_cert_window"  class="contentM_upload" style="box-shadow: 1px 5px 10px #000;">
	<div class="formfonttitle" style="margin-top: 15px; margin-left: 15px;">Import your own certificate</div>
	<div class="formfontdesc" style="margin-left: 15px;">Upload a certificate issued by a certification authority here. Your private key and SSL certificate is necessary.</div>
	<div class="Upload_item">
		<div style="display:table-cell; width: 45%;">Private Key :</div>
		<div style="display:table-cell;"><input type="file" name="file_key" class="input Upload_file"></div>
	</div>
	<div class="Upload_item">
		<div style="display:table-cell; width: 45%;">SSL Certificate :</div>
		<div style="display:table-cell;"><input type="file" name="file_cert" class="input Upload_file"></div>
	</div>
	<div align="center" style="margin-top:30px; padding-bottom:15px;">
		<div style="display:table-cell;"><input class="button_gen" type="button" onclick="hide_upload_window();" id="cancelBtn" value="<#CTL_Cancel#>"></div>
		<div style="display:table-cell; padding-left: 5px;"><input class="button_gen" type="button" onclick="upload_cert_key();" value="<#CTL_ok#>"></div>
	</div>
</div>
</form>
</body>
</html>
