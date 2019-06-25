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
<title><#Web_Title#> - AiCloud 2.0</title>
<link rel="stylesheet" type="text/css" href="index_style.css"> 
<link rel="stylesheet" type="text/css" href="form_style.css">
<link rel="stylesheet" type="text/css" href="app_installation.css">
<script type="text/javascript" src="/state.js"></script>
<script type="text/javascript" src="/popup.js"></script>
<script type="text/javascript" src="/help.js"></script>
<script type="text/javascript" src="/js/jquery.js"></script>
<script type="text/javascript" src="/switcherplugin/jquery.iphone-switch.js"></script>
<script type="text/javascript" src="/form.js"></script>
<script language="JavaScript" type="text/javascript" src="/validator.js"></script>
<script>

if('<% nvram_get("start_aicloud"); %>' == '0')
	location.href = "cloud__main.asp";

<% wanlink(); %>

<% apps_action(); %> //trigger apps_action.

var cloud_status;
var curState = '<% nvram_get("webdav_aidisk"); %>';

var _apps_action = '<% get_parameter("apps_action"); %>';
if(_apps_action == 'cancel')
	_apps_action = '';
	
var apps_array = <% apps_info("asus"); %>;
if(apps_array == ""){
		apps_array =["aicloud", "", "", "no", "no", "", "", "","", "", "", "", ""];
}

<% apps_state_info(); %>

var apps_download_percent_done = 0;

var stoppullstate = 0;
var isinstall = 0;
var installPercent = 1;
var default_apps_array = new Array();
var appnum = 0;
	
var is_cloud_installed = false;
for(var x=0; x < apps_array.length; x++){	//check if AiCloud 2.0 has installed
	if(apps_array[x][0]=='aicloud' && apps_array[x][3]=='yes' && apps_array[x][4]=='yes'){
		is_cloud_installed = true;
	}
}
var ddns_hostname = '<% nvram_get("ddns_hostname_x"); %>';
var https_port = '<% nvram_get("webdav_https_port"); %>';

if(tmo_support)
	var theUrl = "cellspot.router"; 
else
	var theUrl = "router.asus.com";

function initial(){
	show_menu();
	if(is_CN){
		$("#googleplay").hide();
		$("#wandoujia").show();
	}
	else{
		$("#googleplay").show();
		$("#wandoujia").hide();
	}
	document.getElementById("app_state").style.display = "";
	
	if(cloudsync_support){		//aicloud builded in
			divdisplayctrl("none", "none", "none", "");
			document.getElementById('btn_cloud_uninstall').style.display = "none";
			
	}else{		// aicloud ipk
		
		if(_apps_action == '' && !is_cloud_installed){	//setup not yet	
				divdisplayctrl("", "none", "none", "none");
		}		
		
  	if(_apps_action == '' && 
			(apps_state_upgrade == 4 || apps_state_upgrade == "") && 
			(apps_state_enable == 2 || apps_state_enable == "") &&
			(apps_state_update == 2 || apps_state_update == "") && 
			(apps_state_remove == 2 || apps_state_remove == "") &&
			(apps_state_switch == 5 || apps_state_switch == "") && 
			(apps_state_autorun == 4 || apps_state_autorun == "") && 
			(apps_state_install == 5 || apps_state_install == "") &&
			is_cloud_installed){	//setup install is done
					document.getElementById('cloudsetup_movie').style.display = "none";
					divdisplayctrl("none", "none", "none", "");
		}
		else{	//setup status else 
					update_appstate();
		}
	}

	switch(valid_is_wan_ip(wanlink_ipaddr())){
		/* private */
		case 0:
			var aicloud_url = "https://";

			if(sw_mode=="1"){
				//- router mode
				aicloud_url += theUrl;
			}
			else{
			   aicloud_url += '<% nvram_get("lan_ipaddr"); %>';
			}

			if(https_port != 443){
			   aicloud_url += ":" + https_port;
			}

			document.getElementById("accessMethod").innerHTML = "<#AiCloud_enter#> <a id=\"cloud_url\" style=\"font-weight: bolder;text-decoration: underline;\" href=\"" + aicloud_url + "\" target=\"_blank\">" + aicloud_url + "</a>";

			/*
			if(https_port == 443)
				document.getElementById("accessMethod").innerHTML = "<#AiCloud_enter#> <a id=\"cloud_url\" style=\"font-weight: bolder;text-decoration: underline;\" href=\"https://router.asus.com\" target=\"_blank\">https://router.asus.com</a>";
			else{
				document.getElementById("accessMethod").innerHTML = "<#AiCloud_enter#> <a id=\"cloud_url\" style=\"font-weight: bolder;text-decoration: underline;\" href=\"https://router.asus.com\" target=\"_blank\">https://router.asus.com</a>";
				document.getElementById('cloud_url').href = "https://"+ theUrl +":" + https_port;
				document.getElementById('cloud_url').innerHTML = "https://"+ theUrl +":" + https_port;
			}
			*/
			break;
		/* public */
		case 1:
			if('<% nvram_get("ddns_enable_x"); %>' == '1' && ddns_hostname != ''){
				if(https_port == 443) // if the port number of https is 443, hide it
					document.getElementById("accessMethod").innerHTML = "<#AiCloud_enter#> <a style=\"font-weight: bolder;text-decoration: underline;word-break:break-all;\" href=\"https://"+ ddns_hostname + ":"+ https_port +"\" target=\"_blank\">https://"+ ddns_hostname +"</a><br />";
				else
					document.getElementById("accessMethod").innerHTML = "<#AiCloud_enter#> <a style=\"font-weight: bolder;text-decoration: underline;word-break:break-all;\" href=\"https://"+ ddns_hostname + ":"+ https_port +"\" target=\"_blank\">https://"+ ddns_hostname +":"+ https_port +"</a><br />";
				
				document.getElementById("accessMethod").innerHTML += '<#aicloud_disk_case12#>';
			}
			else{
				if(https_port == 443) // if the port number of https is 443, hide it
					document.getElementById("accessMethod").innerHTML = "<#AiCloud_enter#> <a id=\"cloud_url\" style=\"font-weight: bolder;text-decoration: underline;\" href=\"https://router.asus.com\" target=\"_blank\">https://router.asus.com</a>";
				else{
					document.getElementById("accessMethod").innerHTML = "<#AiCloud_enter#> <a id=\"cloud_url\" style=\"font-weight: bolder;text-decoration: underline;\" href=\"https://router.asus.com\" target=\"_blank\">https://router.asus.com</a>";
                                	document.getElementById('cloud_url').href = "https://"+ theUrl +":" + https_port;
	                                document.getElementById('cloud_url').innerHTML = "https://"+ theUrl +":" + https_port;
				}	
				document.getElementById("accessMethod").innerHTML += '<br/><#aicloud_disk_case22#>';
			}
			break;
	}

	if(!rrsut_support)
		document.getElementById("rrsLink").style.display = "none";

	//check DUT is belong to private IP.
	setTimeout("show_warning_message();", 100);
}

var wans_mode ='<% nvram_get("wans_mode"); %>';
var MAX_RETRY_NUM = 5;
var external_ip_retry_cnt = MAX_RETRY_NUM;
function show_warning_message(){
	if(realip_support && wans_mode != "lb"){
		if(realip_state != "2" && external_ip_retry_cnt > 0){
			if( external_ip_retry_cnt == MAX_RETRY_NUM )
				get_real_ip();
			else
				setTimeout("get_real_ip();", 3000);
		}
		else if(realip_state != "2"){
			if(validator.isPrivateIP(wanlink_ipaddr())){
				document.getElementById("privateIP_notes").innerHTML = "<#AiCloud_privateIP_notes#>";
				document.getElementById("privateIP_notes").style.display = "";
			}
		}
		else{
			if(!external_ip){
				document.getElementById("privateIP_notes").innerHTML = "<#AiCloud_privateIP_notes#>";
				document.getElementById("privateIP_notes").style.display = "";
			}
		}
	}
	else if(validator.isPrivateIP(wanlink_ipaddr())){
		document.getElementById("privateIP_notes").innerHTML = "<#AiCloud_privateIP_notes#>";
		document.getElementById("privateIP_notes").style.display = "";
	}
}

function get_real_ip(){
	$.ajax({
		url: 'get_real_ip.asp',
		dataType: 'script',
		error: function(xhr){
			get_real_ip();
		},
		success: function(response){
			external_ip_retry_cnt--;
			show_warning_message();
		}
	});
}

function valid_is_wan_ip(ip_obj){
  // test if WAN IP is a private IP.
  var A_class_start = inet_network("10.0.0.0");
  var A_class_end = inet_network("10.255.255.255");
  var B_class_start = inet_network("172.16.0.0");
  var B_class_end = inet_network("172.31.255.255");
  var C_class_start = inet_network("192.168.0.0");
  var C_class_end = inet_network("192.168.255.255");
  
  var ip_num = inet_network(ip_obj);
  if(ip_num > A_class_start && ip_num < A_class_end)
		return 0;
  else if(ip_num > B_class_start && ip_num < B_class_end)
		return 0;
  else if(ip_num > C_class_start && ip_num < C_class_end)
		return 0;
	else if(ip_num == 0)
		return 0;
  else
		return 1;
}

function inet_network(ip_str){
	if(!ip_str)
		return -1;
	
	var re = /^(\d+)\.(\d+)\.(\d+)\.(\d+)$/;
	if(re.test(ip_str)){
		var v1 = parseInt(RegExp.$1);
		var v2 = parseInt(RegExp.$2);
		var v3 = parseInt(RegExp.$3);
		var v4 = parseInt(RegExp.$4);
		
		if(v1 < 256 && v2 < 256 && v3 < 256 && v4 < 256)
			return v1*256*256*256+v2*256*256+v3*256+v4;
	}
	return -2;
}

function apps_form(_act, _name, _flag){
	cookie.set("apps_last", _name, 1000);
	document.app_form.apps_action.value = _act;
	document.app_form.apps_name.value = _name;
	document.app_form.apps_flag.value = _flag;
	document.app_form.submit();
}

function show_partition(){
 	require(['/require/modules/diskList.js'], function(diskList){
		var htmlcode = "";
		var mounted_partition = 0;
			
		htmlcode += '<div class="formfontdesc" id="usbHint3">Please select an disk partition to install ASUS AiCloud 2.0 , the one you choose will be the system disk.</div>';
		htmlcode += '<div class="formfontdesc" id="usbHint4" style="font-size:12px;">Note: Download Master and AiCloud 2.0 should be installed in the same system disk.</div>';			
		htmlcode += '<table align="center" style="margin:auto;border-collapse:collapse;">';

 		var usbDevicesList = diskList.list();
		for(var i=0; i < usbDevicesList.length; i++){
			for(var j=0; j < usbDevicesList[i].partition.length; j++){
				var all_accessable_size = simpleNum(usbDevicesList[i].partition[j].size-usbDevicesList[i].partition[j].used);
				var all_total_size = simpleNum(usbDevicesList[i].partition[j].size);

				if(usbDevicesList[i].partition[j].status == "unmounted")
					continue;
					
				if(usbDevicesList[i].partition[j].isAppDev){
					if(all_accessable_size > 1)
						htmlcode += '<tr><td class="app_table_radius_left"><div class="iconUSBdisk" onclick="divdisplayctrl(\'none\', \'none\', \'\', \'none\');apps_form(\'install\',\'aicloud\',\''+usbDevicesList[i].partition[j].mountPoint+'\');"></div></td><td class="app_table_radius_right" style="width:200px;">\n';
					else
						htmlcode += '<tr><td class="app_table_radius_left"><div class="iconUSBdisk_noquota"></div></td><td class="app_table_radius_right" style="width:200px;">\n';
					htmlcode += '<div class="app_desc"><b>'+ usbDevicesList[i].partition[j].partName + ' (active)</b></div>';
				}
				else{
					if(all_accessable_size > 1)
						htmlcode += '<tr><td class="app_table_radius_left"><div class="iconUSBdisk" onclick="divdisplayctrl(\'none\', \'none\', \'\', \'none\');apps_form(\'switch\',\'aicloud\',\''+usbDevicesList[i].partition[j].mountPoint+'\');"></div></td><td class="app_table_radius_right" style="width:200px;">\n';
					else
						htmlcode += '<tr><td class="app_table_radius_left"><div class="iconUSBdisk_noquota"></div></td><td class="app_table_radius_right" style="width:200px;">\n';
					htmlcode += '<div class="app_desc"><b>'+ usbDevicesList[i].partition[j].partName + '</b></div>'; 
				}

				if(all_accessable_size > 1)
					htmlcode += '<div class="app_desc"><#Availablespace#>: <b>'+ all_accessable_size+" GB" + '</b></div>'; 
				else
					htmlcode += '<div class="app_desc"><#Availablespace#>: <b>'+ all_accessable_size+" GB <span style=\'color:#FFCC00\'>(Disk quota can not less than 1GB)" + '</span></b></div>'; 

				htmlcode += '<div class="app_desc"><#Totalspace#>: <b>'+ all_total_size+" GB" + '</b></div>'; 
				htmlcode += '</div><br/><br/></td></tr>\n';
				mounted_partition++;
			}
		}

		if(mounted_partition == 0)
			htmlcode += '<tr height="360px"><td colspan="2"><span class="app_name" style="line-height:100%"><#no_usb_found#></span></td></tr>\n';

		document.getElementById("partition_div").innerHTML = htmlcode;
	});
}

function getStyleSheet(cssName, rule) {
	for (i = 0; i < document.styleSheets.length; i++) {
		if (document.styleSheets[i].href.toString().indexOf(cssName) != -1) {
			for (x = 0; x < document.styleSheets[i].cssRules.length; x++) {
				if(document.styleSheets[i].cssRules[x].type == 1) {
					if (document.styleSheets[i].cssRules[x].selectorText.toString() == rule) {
						return document.styleSheets[i].cssRules[x];
					}
				}
			}
		}
	}

	return null;
}
function divdisplayctrl(flag1, flag2, flag3, flag4){
	document.getElementById("cloud_uninstall").style.display = flag1;
	document.getElementById("partition_div").style.display = flag2;
	document.getElementById("app_state").style.display = flag3;
	document.getElementById("cloud_installed").style.display = flag4;

	if(flag1 != "none"){ // AiCloud 2.0 uninstall
		document.getElementById("return_btn").style.display = "none";
		getStyleSheet('index_style', '.tab').style.display = "none";
	}
	else if(flag2 != "none"){ // partition list
		//detectUSBStatusApp();
		show_partition();
		document.getElementById("return_btn").style.display = "";
		//calHeight(1);
		getStyleSheet('index_style', '.tab').style.display = "none";
	}
	else if(flag3 != "none"){ // AiCloud 2.0 installing
		document.getElementById("return_btn").style.display = "none";
		//calHeight(1);
		getStyleSheet('index_style', '.tab').style.display = "none";
	}	
	else if(flag4 != "none"){ // Have AiCloud 2.0 installed
		document.getElementById("return_btn").style.display = "none";
		//calHeight(1);
		getStyleSheet('index_style', '.tab').style.display = "";
	}
	else{
		document.getElementById("return_btn").style.display = "none";		
		//calHeight(0);		
 	}

 	//stoppullstate = 1;
}


function check_wan(){		//Don't need check WAN
	divdisplayctrl("none", "", "none", "none");
	//alert("Please make sure you have connected to internet and reinstall.");	
}

function update_appstate(e){
  $.ajax({
    url: '/update_appstate.asp',
    dataType: 'script',
	
    error: function(xhr){
      update_appstate();
    },
    success: function(response){
			if(stoppullstate == 1)
				return false;
			else if(!check_appstate()){
      	setTimeout("update_appstate();", 1000);
				//calHeight(0);
			}
			else
				update_applist();
		}    
  });
}


function check_appstate(){
	if(_apps_action != "" 
		 && apps_state_upgrade == "" 
		 && apps_state_enable == "" 
		 && apps_state_update == "" 
		 && apps_state_remove == "" 
		 && apps_state_switch == "" 
		 && apps_state_autorun == "" 
		 && apps_state_install == ""){
		return false;
	}

	if((apps_state_upgrade == 4 || apps_state_upgrade == "") 
			&& (apps_state_enable == 2 || apps_state_enable == "") 
			&& (apps_state_update == 2 || apps_state_update == "") 
			&& (apps_state_remove == 2 || apps_state_remove == "") 
			&& (apps_state_switch == 5 || apps_state_switch == "") 
			&& (apps_state_autorun == 4 || apps_state_autorun == "") 
			&& (apps_state_install == 5 || apps_state_install == "")){
		document.getElementById('cloudsetup_movie').style.display = "none";	
		//divdisplayctrl("none", "none", "none", "");
		return true;
	}

	var errorcode;
	var proceed = 0.6;

	divdisplayctrl("none", "none", "", "none");
	document.getElementById('cloudsetup_movie').style.display = "";

	if(apps_state_upgrade != 4 && apps_state_upgrade != ""){ // upgrade error handler
		errorcode = "apps_state_upgrade = " + apps_state_upgrade;
		if(apps_state_error == 1)
			document.getElementById("apps_state_desc").innerHTML = "<#usb_inputerror#>";
		else if(apps_state_error == 2)
			document.getElementById("apps_state_desc").innerHTML = "<#usb_failed_mount#>";
		else if(apps_state_error == 4)
			document.getElementById("apps_state_desc").innerHTML = "<#usb_failed_install#>";
		else if(apps_state_error == 6)
			document.getElementById("apps_state_desc").innerHTML = "<#usb_failed_remote_responding#>";
		else if(apps_state_error == 7)
			document.getElementById("apps_state_desc").innerHTML = "<#usb_failed_upgrade#>";
		else if(apps_state_error == 9)
			document.getElementById("apps_state_desc").innerHTML = "<#usb_failed_unmount#>";
		else if(apps_state_error == 10)
			document.getElementById("apps_state_desc").innerHTML = "<#usb_failed_dev_responding#>";
		else if(apps_state_upgrade == 0)
			document.getElementById("apps_state_desc").innerHTML = "<#usb_initializing#>";
		else if(apps_state_upgrade == 1){
			if(apps_download_percent > 0 && apps_download_percent <= 100){
				document.getElementById("apps_state_desc").innerHTML = apps_download_file + " is downloading.. " + " <b>" + apps_download_percent + "</b> <span style='font-size: 16px;'>%</span>";
				apps_download_percent_done = 0;
			}
			else if(apps_download_percent_done > 5){
				if(installPercent > 99)
					installPercent = 99;
				document.getElementById("loadingicon").style.display = "none";
				document.getElementById("apps_state_desc").innerHTML = "[" + cookie.get("apps_last") + "] " + "<#Excute_processing#> <b>" + Math.round(installPercent) +"</b> <span style='font-size: 16px;'>%</span>";
				installPercent = installPercent + proceed;//*/
			}
			else{
				document.getElementById("apps_state_desc").innerHTML = "<#usb_initializing#>...";
				apps_download_percent_done++;
			}
		}
		else if(apps_state_upgrade == 2)
			document.getElementById("apps_state_desc").innerHTML = "<#usb_uninstalling#>";
		else{
			if(apps_depend_action_target != "terminated" && apps_depend_action_target != "error"){
				if(apps_depend_action_target == "")
					document.getElementById("apps_state_desc").innerHTML = "<b>[" + cookie.get("apps_last") + "] " + "<#Excute_processing#> </b>";
				else
					document.getElementById("apps_state_desc").innerHTML = "<b>[" + cookie.get("apps_last") + "] " + "<#Excute_processing#> </b>"
							+"<br> <span style='font-size: 16px;'> <#Excute_processing#>："+apps_depend_do+"</span>"
							+"<br> <span style='font-size: 16px;'>"+apps_depend_action+"  "+apps_depend_action_target+"</span>"
							;
			}
			else{
				if(installPercent > 99)
					installPercent = 99;
				document.getElementById("loadingicon").style.display = "none";
				document.getElementById("apps_state_desc").innerHTML = "[" + cookie.get("apps_last") + "] " + "<#Excute_processing#> <b>" + Math.round(installPercent) +"</b> <span style='font-size: 16px;'>%</span>";
				installPercent = installPercent + proceed;
			}
		}
	}
	else if(apps_state_enable != 2 && apps_state_enable != ""){
		errorcode = "apps_state_enable = " + apps_state_enable;
		if(apps_state_error == 1)
			document.getElementById("apps_state_desc").innerHTML = "<#usb_failed_unknown#>";
		else if(apps_state_error == 2)
			document.getElementById("apps_state_desc").innerHTML = "<#usb_failed_mount#>";
		else if(apps_state_error == 3)
			document.getElementById("apps_state_desc").innerHTML = "<#usb_failed_create_swap#>";
        else if(apps_state_error == 8)
            document.getElementById("apps_state_desc").innerHTML = "Enable error!";
		else{
			document.getElementById("loadingicon").style.display = "";
			document.getElementById("apps_state_desc").innerHTML = "<#QIS_autoMAC_desc2#>";
		}
	}
	else if(apps_state_update != 2 && apps_state_update != ""){
		errorcode = "apps_state_update = " + apps_state_update;
		if(apps_state_error == 1)
			document.getElementById("apps_state_desc").innerHTML = "<#USB_Application_Preparing#>";
		else if(apps_state_error == 2)
			document.getElementById("apps_state_desc").innerHTML = "<#USB_Application_No_Internet#>";
		else
			document.getElementById("apps_state_desc").innerHTML = "Updating...";
	}
	else if(apps_state_remove != 2 && apps_state_remove != ""){
		errorcode = "apps_state_remove = " + apps_state_remove;
		document.getElementById("apps_state_desc").innerHTML = "<#uninstall_processing#>";
	}
	else if(apps_state_switch != 4 && apps_state_switch != 5 && apps_state_switch != ""){
		errorcode = "apps_state_switch = " + apps_state_switch;
		if(apps_state_error == 1)
			document.getElementById("apps_state_desc").innerHTML = "<#usb_failed_unknown#>";
		else if(apps_state_error == 2)
			document.getElementById("apps_state_desc").innerHTML = "<#usb_failed_mount#>";
		else if(apps_state_switch == 1)
			document.getElementById("apps_state_desc").innerHTML = "<#USB_Application_Stopping#>";
		else if(apps_state_switch == 2)
			document.getElementById("apps_state_desc").innerHTML = "<#USB_Application_Stopwapping#>";
		else if(apps_state_switch == 3)
			document.getElementById("apps_state_desc").innerHTML = "<#USB_Application_Partition_Check#>";
		else
			document.getElementById("apps_state_desc").innerHTML = "<#Excute_processing#>";
	}
	else if(apps_state_autorun != 4 && apps_state_autorun != ""){
		errorcode = "apps_state_autorun = " + apps_state_autorun;
		if(apps_state_error == 1)
			document.getElementById("apps_state_desc").innerHTML = "<#usb_failed_unknown#>";
		else if(apps_state_error == 2)
			document.getElementById("apps_state_desc").innerHTML = "<#usb_failed_mount#>";
		else if(apps_state_autorun == 1)
			document.getElementById("apps_state_desc").innerHTML = "<#USB_Application_disk_checking#>";
		else if(apps_state_install == 2)
			document.getElementById("apps_state_desc").innerHTML = "<#USB_Application_Swap_creating#>";
		else
			document.getElementById("apps_state_desc").innerHTML = "<#Auto_Install_processing#>";
	}
	else if(apps_state_install != 5 && apps_state_error > 0){ // install error handler
		if(apps_state_error == 1)
			document.getElementById("apps_state_desc").innerHTML = "<#usb_inputerror#>";
		else if(apps_state_error == 2)
			document.getElementById("apps_state_desc").innerHTML = "<#usb_failed_mount#>";
		else if(apps_state_error == 3)
			document.getElementById("apps_state_desc").innerHTML = "<#usb_failed_create_swap#>";
		else if(apps_state_error == 4)
			document.getElementById("apps_state_desc").innerHTML = "<#usb_failed_install#>";
		else if(apps_state_error == 5)
			document.getElementById("apps_state_desc").innerHTML = "<#usb_failed_connect_internet#>";
		else if(apps_state_error == 6)
			document.getElementById("apps_state_desc").innerHTML = "<#usb_failed_remote_responding#>";
		else if(apps_state_error == 7)
			document.getElementById("apps_state_desc").innerHTML = "<#usb_failed_upgrade#>";
		else if(apps_state_error == 9)
			document.getElementById("apps_state_desc").innerHTML = "<#usb_failed_unmount#>";
		else if(apps_state_error == 10)
			document.getElementById("apps_state_desc").innerHTML = "<#usb_failed_dev_responding#>";

		isinstall = 0;
	}
	else if(apps_state_install != 5 && apps_state_install != ""){
		isinstall = 1;
		errorcode = "_apps_state_install = " + apps_state_install;

		if(apps_state_install == 0)
			document.getElementById("apps_state_desc").innerHTML = "<#usb_partitioning#>";
		else if(apps_state_install == 1)
			document.getElementById("apps_state_desc").innerHTML = "<#USB_Application_disk_checking#>";
		else if(apps_state_install == 2)
			document.getElementById("apps_state_desc").innerHTML = "<#USB_Application_Swap_creating#>";
		else if(apps_state_install == 3){
			if(apps_download_percent > 0 && apps_download_percent <= 100){
				document.getElementById("apps_state_desc").innerHTML = apps_download_file + " is downloading.. " + " <b>" + apps_download_percent + "</b> <span style='font-size: 16px;'>%</span>";
				apps_download_percent_done = 0;
			}
			else if(apps_download_percent_done > 5){
				if(installPercent > 99)
					installPercent = 99;
				document.getElementById("loadingicon").style.display = "none";
				document.getElementById("apps_state_desc").innerHTML = "[" + cookie.get("apps_last") + "] " + "<#Excute_processing#> <b>" + Math.round(installPercent) +"</b> <span style='font-size: 16px;'>%</span>";
				installPercent = installPercent + proceed;//*/
			}
			else{
				document.getElementById("apps_state_desc").innerHTML = "<#usb_initializing#>...";
				apps_download_percent_done++;
			}
		}
		else{
			if(apps_depend_action_target != "terminated" && apps_depend_action_target != "error"){
				if(apps_depend_action_target == "")
					document.getElementById("apps_state_desc").innerHTML = "<b>[" + cookie.get("apps_last") + "] " + "<#Excute_processing#> </b>";
				else
					document.getElementById("apps_state_desc").innerHTML = "<b>[" + cookie.get("apps_last") + "] " + "<#Excute_processing#> </b>"
							+"<br> <span style='font-size: 16px;'> <#Excute_processing#>："+apps_depend_do+"</span>"
							+"<br> <span style='font-size: 16px;'>"+apps_depend_action+"  "+apps_depend_action_target+"</span>"
							;
			}
			else{
				if(installPercent > 99)
					installPercent = 99;
				document.getElementById("loadingicon").style.display = "none";
				document.getElementById("apps_state_desc").innerHTML = "[" + cookie.get("apps_last") + "] " + "<#Excute_processing#> <b>" + Math.round(installPercent) +"</b> <span style='font-size: 16px;'>%</span>";
				installPercent = installPercent + proceed;
			}
		}
	}
	else{
		document.getElementById("loadingicon").style.display = "";
		document.getElementById("apps_state_desc").innerHTML = "<#QIS_autoMAC_desc2#>";
	}
	
	if(apps_state_error != 0){
		document.getElementById("return_btn").style.display = "";
		document.getElementById("loadingicon").style.display = "none";
		stoppullstate = 1;
	}
	else
		document.getElementById("return_btn").style.display = "none";

	document.getElementById("apps_state_desc").innerHTML += '<span class="app_action" onclick="apps_form(\'cancel\',\'\',\'\');">(<#CTL_Cancel#>)</span>';
	return false;
}


function update_applist(e){
  $.ajax({
    url: '/update_applist.asp',
    dataType: 'script',
	
    error: function(xhr){
      update_applist();
    },
    success: function(response){
			if(isinstall > 0 && cookie.get("apps_last") == "aicloud"){
				document.getElementById('cloudsetup_movie').style.display = "none";
				setTimeout('divdisplayctrl("none", "none", "none", "");', 100);
			}
			else{
				document.getElementById('cloudsetup_movie').style.display = "none";
				setTimeout('show_partition();', 100);
				setTimeout('divdisplayctrl("", "none", "none", "none");', 100);
				//setTimeout('show_apps();', 100);
			}
		}    
  });
}

</script>
</head>
<body onload="initial();" onunload="return unload_body();">
<div id="TopBanner"></div>
<div id="Loading" class="popup_bg"></div>
	<table cellpadding="5" cellspacing="0" id="dr_sweet_advise" class="dr_sweet_advise" align="center">
	</table>
	<!--[if lte IE 6.5]><iframe class="hackiframe"></iframe><![endif]-->
	</div>
<iframe name="hidden_frame" id="hidden_frame" width="0" height="0" frameborder="0" scrolling="no"></iframe>
<form method="post" name="app_form" action="/cloud_main.asp">
<input type="hidden" name="preferred_lang" value="<% nvram_get("preferred_lang"); %>" disabled>
<input type="hidden" name="firmver" value="<% nvram_get("firmver"); %>" disabled>
<input type="hidden" name="apps_action" value="">
<input type="hidden" name="apps_path" value="">
<input type="hidden" name="apps_name" value="">
<input type="hidden" name="apps_flag" value="">
</form>
<form method="post" name="form" action="/start_apply.htm" target="hidden_frame">
<input type="hidden" name="preferred_lang" id="preferred_lang" value="<% nvram_get("preferred_lang"); %>">
<input type="hidden" name="firmver" value="<% nvram_get("firmver"); %>">
<input type="hidden" name="current_page" value="cloud_main.asp">
<input type="hidden" name="next_page" value="cloud_main.asp">
<input type="hidden" name="action_mode" value="">
<input type="hidden" name="action_script" value="">
<input type="hidden" name="action_wait" value="">
<input type="hidden" name="enable_cloudsync" value="<% nvram_get("enable_cloudsync"); %>">
<input type="hidden" name="enable_webdav" value="<% nvram_get("enable_webdav"); %>">
<input type="hidden" name="webdav_aidisk" value="<% nvram_get("webdav_aidisk"); %>">
<input type="hidden" name="webdav_proxy" value="<% nvram_get("webdav_proxy"); %>">

<table border="0" align="center" cellpadding="0" cellspacing="0" class="content">
	<tr>
		<td valign="top" width="17">&nbsp;</td>
		<!--=====Beginning of Main Menu=====-->
		<td valign="top" width="202">
			<div id="mainMenu"></div>
			<div id="subMenu"></div>
		</td>
		<td valign="top">
			<div id="tabMenu" class="submenuBlock"></div>
<!--==============Beginning of hint content=============-->
			<table width="98%" border="0" align="left" cellpadding="0" cellspacing="0">
			  <tr>
					<td align="left" valign="top">
					  <table width="100%" border="0" cellpadding="5" cellspacing="0" class="FormTitle" id="FormTitle">
							<tbody>
							<tr>
							  <td bgcolor="#4D595D" valign="top">
									<div>&nbsp;</div>
									<div class="formfonttitle">AiCloud 2.0</div>									
									<div><img id="return_btn" onclick="location.href='/cloud_main.asp'" align="right" style="cursor:pointer;position:absolute;margin-left:690px;margin-top:-45px;" title="Back to AiCloud 2.0" src="/images/backprev.png" onMouseOver="this.src='/images/backprevclick.png'" onMouseOut="this.src='/images/backprev.png'"></div>
									<div style="margin:10px 0 10px 5px;" class="splitLine"></div>
									<div id="cloud_uninstall" style="display:none;">
   										<table>	
  										<tr>
   												<td><div class="formfontdesc" id="usbHint"><#AiCloud_maintext_note#></div></td> 
  										</tr>
											<tr>
   												<td><div class="formfontdesc" id="usbHint2"><#Learn_more#> : <a href="http://www.asus.com/search/results.aspx?SearchKey=AiCloud&SearchType=FAQ&IsSupport=True&Page=1" target="_blank" style="color:#FC0;text-decoration: underline; font-family:Lucida Console;">GO</a></div></td> 
  										</tr>  	   
  										<tr>
   												<td align="center" width="740px" height="60px">
													<div id="gotonext">
  														<div class="titlebtn" style="margin-top: 50px;margin-left:300px;_margin-left:150px;cursor:pointer;" align="center"><span id="settingBtn" style="*width:140px;" onClick="check_wan();">Install</span></div>
													</div>
  												</td>
  										</tr>  
   										</table>										
									</div>
									<div id="partition_div" style="display:none;"></div>
									<div id="cloudsetup_movie" style="box-shadow: 2px 2px 15px #222;display:none;width:349px;height:193px;margin-left:165px;margin-top:40px;background:url(images/setup.jpg) no-repeat center;cursor:pointer" onClick="window.open('http://www.youtube.com/watch?v=1zIVzl1h8P4')"></div>
									<div id="app_state" class="app_state" style="display:none;"><span id="apps_state_desc">Loading APP list...</span><img id="loadingicon" style="margin-left:5px;" src="/images/InternetScan.gif"></div>
									<div id="cloud_installed" style="display:none;">
									<table width="100%" height="550px" style="border-collapse:collapse;">

									  <tr class="block_bg">
									    <td colspan="5" class="cloud_main_radius">
												<div style="padding:10px;width:95%;font-style:italic;font-size:14px;">
												<#AiCloud_maintext_note#>
													<br/><br/>
													<table width="100%" >
														<tr>
															<td>
																<ul style="margin: 0px;padding-left:15px;" >
																	<li style="margin-top:-5px;">
																 	<div id="accessMethod"></div>
																 	</li>
																	<li style="margin-top:-5px;">
																	 <#FAQ_Find#> <a style="font-weight: bolder;text-decoration: underline;" href="http://www.asus.com/search/results.aspx?SearchKey=AiCloud&SearchType=FAQ&IsSupport=True&Page=1" target="_blank">GO</a>
																	</li>
																</ul>
															</td>
															<td>							
																<a id="googleplay" href="https://play.google.com/store/apps/details?id=com.asustek.aicloud" target="_blank">
																	<div style="width:172px;height:51px;background:url('images/cloudsync/googleplay.png') no-repeat;background-size:75%;"></div>
																</a>												
																<a id="wandoujia" href="http://www.wandoujia.com/apps/com.asus.aihome" target="_blank" style="display:none">
																	<div style="width:130px;height:51px;text-align: center;line-height:51px;line-height: 51px;font-size: 20px;font-weight: bold;text-decoration: underline;">豌豆荚</div>
																</a>			
																<a href="https://itunes.apple.com/us/app/aicloud-lite/id527118674" target="_blank">
																	<div style="width:172px;height:51px;background:url('images/cloudsync/AppStore.png') no-repeat;background-size:75%;"></div>
																</a>	
															</td>
														</tr>
													</table>
												</div>
												<div id="privateIP_notes" class="formfontdesc" style="display:none; color:#FFCC00; padding:10px;"></div>
											</td>
										</tr>

										<tr height="10px">
											<td colspan="3">
											</td>
									  </tr>

									  <tr class="block_bg block_line" width="235px">
									    <td class="cloud_main_radius_left" width="20%" height="50px">
												<div style="padding:10px;" align="center"><img src="/images/cloudsync/001.png">
													<div align="center" style="margin-top:10px;font-size: 18px;text-shadow: 1px 1px 0px black;"><#Cloud_Disk#></div>
												</div>
											</td>

									    <td width="6px">
												<div class="line_vertical"></div>
											</td>

									    <td width="1px"></td>

									    <td>
											<div style="padding:10px;width:95%;font-style:italic;font-size:14px;">
												<#aicloud_disk_desc#>												
											</div>
										</td>

									    <td class="cloud_main_radius_right" width="100px">
												<div align="center" class="left" style="width:94px; float:left; cursor:pointer;" id="radio_clouddisk_enable"></div>
												<div id="iphone_switch_container" class="iphone_switch_container" style="height:32px; width:74px; position: relative; overflow: hidden">
												<script type="text/javascript">
													$('#radio_clouddisk_enable').iphoneSwitch('<% nvram_get("webdav_aidisk"); %>', 
														 function(){
															if(window.scrollTo)
																window.scrollTo(0,0);
															curState = 1;
															document.form.webdav_aidisk.value = 1;
															document.form.enable_webdav.value = 1;
															FormActions("start_apply.htm", "apply", "restart_webdav", "3");
															showLoading();
															document.form.submit();
														 },
														 function() {
															document.form.webdav_aidisk.value = 0;
															if(document.form.webdav_proxy.value == 0)
																document.form.enable_webdav.value = 0;
															FormActions("start_apply.htm", "apply", "restart_webdav", "3");
															showLoading();	
															document.form.submit();
														 }
													);
												</script>
												</div>	
											</td>
									  </tr>

									  <tr height="10px">
											<td colspan="3">
											</td>
									  </tr>
										
									  <tr class="block_bg block_line">
									    <td class="cloud_main_radius_left" width="20%" height="50px">
												<div style="padding:10px;" align="center"><img src="/images/cloudsync/002.png">
													<div align="center" style="margin-top:10px;font-size: 18px;text-shadow: 1px 1px 0px black;"><#Smart_Access#></div>
												</div>
											</td>
									    <td>
												<div class="line_vertical"></div>
											</td>
									    <td>
												&nbsp;
											</td>
									    <td>
												<div style="padding:10px;width:95%;font-style:italic;font-size:14px;">
													<#smart_access_desc#>
												</div>
											</td>
									    <td class="cloud_main_radius_right" width="100">
												<div align="center" class="left" style="width:94px; float:left; cursor:pointer;" id="radio_smartAccess_enable"></div>
												<div class="iphone_switch_container" style="height:32px; width:74px; position: relative; overflow: hidden">
												<script type="text/javascript">
													$('#radio_smartAccess_enable').iphoneSwitch('<% nvram_get("webdav_proxy"); %>', 
														 function() {
															document.form.webdav_proxy.value = 1;
															document.form.enable_webdav.value = 1;
															FormActions("start_apply.htm", "apply", "restart_webdav", "3");
															showLoading();
															document.form.submit();
														 },
														 function() {
															document.form.webdav_proxy.value = 0;
															if(document.form.webdav_aidisk.value == 0)
																document.form.enable_webdav.value = 0;
															FormActions("start_apply.htm", "apply", "restart_webdav", "3");
															showLoading();
															document.form.submit();
														 }
													);
												</script>
												</div>
											</td>
									  </tr>

									  <tr height="10px">
											<td colspan="3">
											</td>
									  </tr>

									  <tr class="block_bg">
									    <td class="cloud_main_radius_left" width="20%" height="50px">
												<div style="padding:10px;" align="center">
													<img src="/images/cloudsync/003.png">
													<div align="center" style="margin-top:10px;font-size: 18px;text-shadow: 1px 1px 0px black;"><#smart_sync#></div>
												</div>
											</td>
									    <td>
												<div class="line_vertical"></div>
											</td>
									    <td>
												&nbsp;
											</td>
									    <td width="">
												<div style="padding:10px;width:95%;font-style:italic;font-size:14px;">
													<#smart_sync_desc#>
												</div>
											</td>
									    <td class="cloud_main_radius_right" width="100">
						  					<input name="button" type="button" class="button_gen" onclick="location.href='/cloud_sync.asp'" value="<#btn_go#>"/>
											</td>
									  </tr>
									  <tr>
									  	<td colspan="5" id="btn_cloud_uninstall">
												<div class="apply_gen" style="margin-top:20px;">
													<input class="button_gen" onclick="apps_form('remove','aicloud','');" type="button" value="Uninstall"/>
			            			</div>			            
			            		</td>
			            	</tr>		
									</table>
									</div>

							  </td>
							</tr>				
							</tbody>	
				  	</table>			
					</td>
				</tr>
			</table>
		</td>
		<td width="20"></td>
	</tr>
</table>
<div id="footer"></div>
</form>

</body>
</html>
