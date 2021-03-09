<!DOCTYPE html PUBLIC "-//W3C//DTD XHTML 1.0 Transitional//EN" "http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd">
<html xmlns="http://www.w3.org/1999/xhtml">
<html xmlns:v>
<head>
<meta http-equiv="X-UA-Compatible" content="IE=Edge"/>
<meta http-equiv="Content-Type" content="text/html; charset=utf-8">
<meta HTTP-EQUIV="Pragma" CONTENT="no-cache">
<meta HTTP-EQUIV="Expires" CONTENT="-1">
<link rel="shortcut icon" href="images/favicon.png">
<link rel="icon" href="images/favicon.png"><title><#Web_Title#> - <#Game_Boost#></title>
<link rel="stylesheet" type="text/css" href="index_style.css">
<link rel="stylesheet" type="text/css" href="form_style.css">
<link rel="stylesheet" type="text/css" href="usp_style.css">
<link rel="stylesheet" type="text/css" href="css/basic.css">
<link rel="stylesheet" type="text/css" href="css/triLvGameAcceleration.css">
<script type="text/javascript" src="/state.js"></script>
<script type="text/javascript" src="/popup.js"></script>
<script type="text/javascript" src="/help.js"></script>
<script type="text/javascript" src="/js/jquery.js"></script>
<script type="text/javascript" src="/form.js"></script>
<script type="text/javascript" src="/js/httpApi.js"></script>
<script language="JavaScript" type="text/javascript" src="/js/asus_eula.js"></script>
<style>
body{
    margin: 0;
    color: #FFFFFF;
}
</style>
<script>
var ctf_disable = '<% nvram_get("ctf_disable"); %>';
var ctf_fa_mode = '<% nvram_get("ctf_fa_mode"); %>';
var label_mac = <% get_label_mac(); %>;
var bwdpi_app_rulelist = "<% nvram_get("bwdpi_app_rulelist"); %>".replace(/&#60/g, "<");
var CNSku = in_territory_code("CN");
function initial(){
	show_menu();
	httpApi.faqURL("1010951", function(url){document.getElementById("faq").href=url;});
	if((document.form.qos_enable.value == '1') && (document.form.qos_type.value == '1') && (bwdpi_app_rulelist.indexOf('game') != -1)){
		document.getElementById("game_boost_enable").checked = true;
	}
	else{
		document.getElementById("game_boost_enable").checked = false;
	}

	if(uu_support){
		$('#last_line').show();
		$('#uu_field').show();
	}

	if(!ASUS_EULA.status("tm")){
		ASUS_EULA.config(eula_confirm, cancel);
	}	
}

function sign_eula(){
	if(document.getElementById("game_boost_enable").checked){
		if(!reset_wan_to_fo.check_status()) {
			document.getElementById("game_boost_enable").checked = false;
			return false;
		}
	}

	if(ASUS_EULA.check("tm")){
		check_game_boost();
	}
}

function check_game_boost(){
	if(document.getElementById("game_boost_enable").checked){
		document.form.qos_enable.value = '1';
		document.form.qos_type.value = '1';
		document.form.bwdpi_app_rulelist.disabled = false;
		document.form.bwdpi_app_rulelist.value = "9,20<8<4<0,5,6,15,17<4,13<13,24<1,3,14<7,10,11,21,23<game";
	}
	else{
		document.form.qos_enable.value = '0';
		document.form.bwdpi_app_rulelist.disabled = true;
	}

	if(ctf_disable == 1){
		document.form.action_script.value = "restart_qos;restart_firewall";
	}
	else{
		if(ctf_fa_mode == "2"){
			FormActions("start_apply.htm", "apply", "reboot", "<% get_default_reboot_time(); %>");
		}
		else{
			if(document.form.qos_type.value == 0)
				FormActions("start_apply.htm", "apply", "reboot", "<% get_default_reboot_time(); %>");
			else{
				document.form.action_script.value = "restart_qos;restart_firewall";
			}
		}
	}

	if(reset_wan_to_fo.change_status)
		reset_wan_to_fo.change_wan_mode(document.form);

	document.form.submit();
}

function eula_confirm(){
	document.form.TM_EULA.value = 1;
	document.form.action_wait.value = "15";
	check_game_boost();
}

function cancel(){
	refreshpage();
}
var siteInfo = ['https://www.asus.com/support/FAQ/1042778/',
	     		'Advanced_WTFast_Content.asp',
				'QoS_EZQoS.asp'];
function redirectSite(url){
	window.open(url, '_blank');
}

function uuRegister(mac){
	var _mac = mac.toLowerCase();
	window.open('https://router.uu.163.com/asus/pc.html#/acce?gwSn=' + _mac + '&type=asuswrt-merlin', '_blank');
}
</script>
</head>
<body onload="initial();" onunload="unload_body();">
<div id="TopBanner"></div>
<div id="Loading" class="popup_bg"></div>
<div id="hiddenMask" class="popup_bg" style="z-index:999;">
	<table cellpadding="5" cellspacing="0" id="dr_sweet_advise" class="dr_sweet_advise" align="center"></table>
	<!--[if lte IE 6.5.]><script>alert("<#ALERT_TO_CHANGE_BROWSER#>");</script><![endif]-->
</div>
<iframe name="hidden_frame" id="hidden_frame" width="0" height="0" frameborder="0" scrolling="no"></iframe>
<form method="post" name="form" action="/start_apply.htm" target="hidden_frame">
<input type="hidden" name="current_page" value="GameBoost.asp">
<input type="hidden" name="next_page" value="GameBoost.asp">
<input type="hidden" name="preferred_lang" id="preferred_lang" value="<% nvram_get("preferred_lang"); %>">
<input type="hidden" name="firmver" value="<% nvram_get("firmver"); %>">
<input type="hidden" name="action_mode" value="apply">
<input type="hidden" name="action_script" value="restart_qos;restart_firewall">
<input type="hidden" name="action_wait" value="5">
<input type="hidden" name="qos_enable" value="<% nvram_get("qos_enable"); %>">
<input type="hidden" name="qos_type" value="<% nvram_get("qos_type"); %>">
<input type="hidden" name="bwdpi_app_rulelist" value="<% nvram_get("bwdpi_app_rulelist"); %>">
<input type="hidden" name="TM_EULA" value="<% nvram_get("TM_EULA"); %>">
</form>
<div>
	<table class="content" align="center" cellspacing="0" >
		<tr>
			<td width="17">&nbsp;</td>
			<!--=====Beginning of Main Menu=====-->
			<td valign="top" width="202">
				<div id="mainMenu"></div>
				<div id="subMenu"></div>
			</td>
			<td valign="top">
				<div id="tabMenu" style="*margin-top: -160px;"></div>
				<div class="border-container border1-container">
					<div class="border-corner border-corner-top-left"></div>
					<div class="border-corner border-corner-bottom-left"></div>
					<div class="border-bar"></div>

					<div class="border1-modelProduct"></div>
					<div class="flexbox  title-container">
						<div class="title-symbol"></div>
						<div class="title-content"><#Game_Boost_acceleration#></div>
					</div>
					<div class="description-container"><#Game_Boost_acceleration_desc#></div>
					<div class="flexbox flex-j-center border1-LV-container">
						<div class="flexbox flex-a-center triLv-container">
							<div>
								<div class="triLv-level"><#AiProtection_level_th#></div>
								<div class="flexbox flex-a-center flex-j-center triLv-border">
									<div class="triLv-step">1</div>
								</div>
							</div>
							<div class="triLv-desc"><#Game_Port_Prio#></div>
						</div>

						<div class="flexbox flex-a-center triLv-container">
							<div>
								<div class="triLv-level"><#AiProtection_level_th#></div>
								<div class="flexbox flex-a-center flex-j-center triLv-border">
									<div class="triLv-step">2</div>
								</div>
							</div>
							<div class="triLv-desc"><#Game_Packet_Prio#></div>
						</div>

						<div class="flexbox flex-a-center triLv-container">
							<div>
								<div class="triLv-level"><#AiProtection_level_th#></div>
								<div class="flexbox flex-a-center flex-j-center triLv-border">
									<div class="triLv-step">3</div>
								</div>
							</div>
							<div class="triLv-desc"><#Game_Server_acceleration#></div>
						</div>
					</div>
				</div>

				<div class="border-container border2-container">
					<div class="border-corner border-corner-top-left"></div>
					<div class="border-corner border-corner-bottom-left"></div>
					<div class="border-bar"></div>

					<div class="card-boder">
						<div class="flexbox border2-title">
							<div class="flexbox title-container border2-title-container">
								<div class="title-symbol"></div>
								<div class="title-content border2-title-desc"><#Game_Port_Prio#></div>
							</div>
							<div>
								<div class="triLv-level"><#AiProtection_level_th#></div>
								<div class="flexbox flex-a-center flex-j-center triLv-border">
									<div class="triLv-step">1</div>
								</div>
							</div>
						</div>
						<div class="flexbox flex-a-center" style="">
							<div class="content-image-container Game-D-device-image"></div>
							<div class="content-divide-line"></div>
							<div class="flex-as-start content-desc-container">
								<div class="flexbox flex-a-center">
									<div class="content-desc-title"><#Game_devices#></div>
								</div>
								<div class="content-desc"><#Game_devices_desc#></div>
							</div>
							<div class="content-divide-line"></div>
						</div>
						<div class="divide-line"></div>
						<div class="flexbox flex-a-center">
							<div class="content-image-container ROG-D-Opt-image"></div>
							<div class="content-divide-line"></div>
							<div class="flex-as-start content-desc-container">
								<div class="flexbox flex-a-center">
									<div class="content-desc-title">ROG First</div>
									<div class="content-divide-line-sm"></div>
									<div class="content-detail">
										<a href="https://www.asus.com/support/FAQ/1036877" target="_blank">FAQ</a>
									</div>
								</div>
								<div class="content-desc"><#Game_First_desc#></div>
							</div>
							<div class="content-divide-line"></div>
							<div class="content-action-container" onclick="redirectSite(siteInfo[0]);">
								<div class="button-container button-container-sm">
									<div class="button-icon icon-go"></div>
									<div class="button-text"><#btn_go#></div>
								</div>
							</div>
						</div>
					</div>
					<div class="card-boder">
						<div class="flexbox border2-title">
							<div class="flexbox title-container border2-title-container">
								<div class="title-symbol"></div>
								<div class="title-content border2-title-desc"><#Game_Packet_Prio#></div>
							</div>
							<div>
								<div class="triLv-level"><#AiProtection_level_th#></div>
								<div class="flexbox flex-a-center flex-j-center triLv-border">
									<div class="triLv-step">2</div>
								</div>
							</div>
						</div>
						<div class="flexbox flex-a-center">
							<div class="content-image-container Game-Packet-Pri-iamge">
								<div class="Game-Packet-Pri-modelProduct"></div>
							</div>
							<div class="content-divide-line"></div>
							<div class="flex-as-start content-desc-container">
								<div class="flexbox flex-a-center">
									<div class="content-desc-title"><#Game_Boost#></div>
									<div class="content-divide-line-sm"></div>
									<div class="content-detail">
										<a id="faq" href="" target="_blank">FAQ</a>
									</div>
								</div>
								<div class="content-desc"><#Game_Boost_desc1#></div>
							</div>
							<div class="content-divide-line"></div>
							<div class="content-action-container">
								<div class="gameBoost-enable-desc"><#BoostKey_enable#></div>
								<div class="switch-button-container gameBoost-enable-offset" >
									<label for="game_boost_enable">
										<input type="checkbox" id="game_boost_enable" class="switch-button" onchange="sign_eula();">
										<div class="switch-button-bg"></div>
										<div class="switch-button-circle"></div>
									</label>
								</div>
								<div class="content-action-container" onclick="redirectSite(siteInfo[2]);">
									<div class="button-container button-container-sm">
										<div class="button-icon icon-go"></div>
										<div class="button-text"><#btn_go#></div>
									</div>
								</div>
							</div>
						</div>
					</div>

					<div class="card-boder">
						<div class="flexbox border2-title">
							<div class="flexbox title-container border2-title-container">
								<div class="title-symbol"></div>
								<div class="title-content border2-title-desc"><#Game_Server_acceleration#></div>
							</div>
							<div>
								<div class="triLv-level"><#AiProtection_level_th#></div>
								<div class="flexbox flex-a-center flex-j-center triLv-border">
									<div class="triLv-step">3</div>
								</div>
							</div>
						</div>
						<div class="flexbox flex-a-center">
							<div class="content-image-container Game-wtfast-image"></div>
							<div class="content-divide-line"></div>
							<div class="flex-as-start content-desc-container">
								<div class="flexbox flex-a-center">
									<div class="content-desc-title">WTFast&reg;</div>
									<div class="content-divide-line-sm"></div>
									<div class="content-detail">
										<a href="https://www.wtfast.com/routers" target="_blank">FAQ</a>
									</div>
								</div>
								<div class="content-desc"><#Game_WTFast_desc#></div>
							</div>
							<div class="content-divide-line"></div>
							<div class="content-action-container" onclick="redirectSite(siteInfo[1]);">
								<div class="button-container button-container-sm">
									<div class="button-icon icon-go"></div>
									<div class="button-text"><#btn_go#></div>
								</div>
							</div>
						</div>
						<div id="last_line" class="divide-line" style="display:none"></div>
						<div id="uu_field" class="flexbox flex-a-center" style="display:none">
							<div class="content-image-container Game-uu-image"></div>
							<div class="content-divide-line"></div>
							<div class="flex-as-start content-desc-container">
								<div class="flexbox flex-a-center">
									<div class="content-desc-title">网易UU加速器</div>
									<div class="content-divide-line-sm"></div>
									<div class="content-detail">
										<a href="https://uu.163.com/router/" target="_blank">FAQ</a>
									</div>
								</div>
								<div class="content-desc">UU路由器插件为三大主机PS4、Switch、Xbox One提供加速。可实现多台主机同时加速，NAT类型All Open。畅享全球联机超快感！</div>
							</div>
							<div class="content-divide-line"></div>
							<div class="content-action-container" onclick="uuRegister(label_mac);">
								<div class="button-container button-container-sm">
									<div class="button-icon icon-go"></div>
									<div class="button-text"><#btn_go#></div>
								</div>
							</div>
						</div>
					</div>
				</div>	
			</td>
			<td width="20" align="center" valign="top"></td>
		</tr>
	</table>
</div>
<div id="footer"></div>
</body>
</html>
