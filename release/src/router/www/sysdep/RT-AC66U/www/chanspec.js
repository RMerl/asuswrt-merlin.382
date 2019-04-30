var country = '';
if(<% nvram_get("wl_unit"); %> == '1')
	country = '<% nvram_get("wl1_country_code"); %>';
else		
	country = '<% nvram_get("wl0_country_code"); %>';

var wl1 = {
	"channel_20m": [],
	"channel_40m": []
}
if(band5g_11ac_support){
	wl1["channel_80m"] = new Array();
}

var _chanspecs_5g =  JSON.parse('<% chanspecs_5g(); %>');

for(i=0;i<_chanspecs_5g.length;i++){
	if(_chanspecs_5g[i].indexOf("/80") != -1){
		wl1.channel_80m.push(_chanspecs_5g[i]);
	}
	else if(_chanspecs_5g[i].indexOf("u")!= -1 || _chanspecs_5g[i].indexOf("l") != -1){
		wl1.channel_40m.push(_chanspecs_5g[i]);
	}
	else{
		wl1.channel_20m.push(_chanspecs_5g[i]);
	}

	if(based_modelid == "RT-AC87U"){
		ch80m = JSON.parse('<% channel_list_5g_80m(); %>');
		wl1.channel_80m.push("0");
		for(i=0;i<ch80m.length;i++){
			wl1.channel_80m.push(ch80m[i] + "/80");
		}

		ch40m = JSON.parse('<% channel_list_5g_40m(); %>');
		wl1.channel_40m.push("0");
		for(i=0;i<ch40m.length;i++){
			wl1.channel_40m.push(wlextchannel_fourty(ch40m[i]));
		}
	}
}
if(wl_info.band5g_2_support){
	var wl2 = {
		"channel_20m": [],
		"channel_40m": [],
		"channel_80m": []
	}

	var _chanspecs_5g_2 =  JSON.parse('<% chanspecs_5g_2(); %>');
	for(i=0;i<_chanspecs_5g_2.length;i++){
		if(_chanspecs_5g_2[i].indexOf("/80") != -1){
			wl2.channel_80m.push(_chanspecs_5g_2[i]);
		}
		else if(_chanspecs_5g_2[i].indexOf("u")!= -1 || _chanspecs_5g_2[i].indexOf("l") != -1){
			wl2.channel_40m.push(_chanspecs_5g_2[i]);
		}
		else{
			wl2.channel_20m.push(_chanspecs_5g_2[i]);
		}
	}	
}

function wl_chanspec_list_change(){
	var phytype = "n";
	var band = document.form.wl_unit.value;
	var bw_cap = document.form.wl_bw.value;
	var bw_cap_ori = '<% nvram_get("wl_bw"); %>';
	var chanspecs = new Array(0);
	var chanspecs_string = new Array(0);
	var cur = 0;
	var sel = 0;
	var cur_control_channel = 0;
	var extend_channel = new Array();
	var cur_extend_channel = 0;			//current extension channel
	var channel_ori = '<% nvram_get("wl_chanspec"); %>';

	if(country == ""){
		country = prompt("The Country Code is not exist! Please enter Country Code.", "");
	}

	/* Save current chanspec */
	cur = '<% nvram_get("wl_chanspec"); %>';
	if (phytype == "a") {	// a mode
		chanspecs = new Array(0); 
	}
	else if (phytype == "n"){ // n mode
		if (band == "1"){ // ---- 5 GHz
			if(wl_channel_list_5g instanceof Array && wl_channel_list_5g != ["0"]){	//With wireless channel 5g hook or return not ["0"]
				if(based_modelid == "RT-AC87U"){
					if(document.form.wl_bw.value==1){
						wl_channel_list_5g = JSON.parse('<% channel_list_5g_20m(); %>');
					}else if(document.form.wl_bw.value==2){
						wl_channel_list_5g = JSON.parse('<% channel_list_5g_40m(); %>');
					}else if(document.form.wl_bw.value==3){
						wl_channel_list_5g = JSON.parse('<% channel_list_5g_80m(); %>');
					}else{		//exception, RT-AC87U without 'auto'
						wl_channel_list_5g = JSON.parse('<% channel_list_5g(); %>');
					}
				}else{
					wl_channel_list_5g = JSON.parse('<% channel_list_5g(); %>');
				}	

				extend_channel = ["<#Auto#>"];		 // for 5GHz, extension channel always displays Auto
				extend_channel_value = [""];
				if(bw_cap == "0"){	// 20/40/80 MHz (auto)
					document.getElementById('wl_nctrlsb_field').style.display = "";
		loop_auto: for(i=0; i<wl_channel_list_5g.length; i++){
						var _cur_channel = parseInt(wl_channel_list_5g[i]);
						if(band5g_11ac_support && document.form.wl_nmode_x.value != 1){
							for(j=0;j<wl1.channel_80m.length;j++){
								if(wl1.channel_80m[j].indexOf(_cur_channel) != -1){
									wl_channel_list_5g[i] = _cur_channel + "/80";
									continue loop_auto;
								}
							}
						}

						for(j=0;j<wl1.channel_40m.length;j++){
							if(wl1.channel_40m[j].indexOf(_cur_channel) != -1){
								wl_channel_list_5g[i] = wlextchannel_fourty(_cur_channel);
								continue loop_auto;
							}
						}

						for(j=0;j<wl1.channel_20m.length;j++){
							if(wl1.channel_20m[j].indexOf(_cur_channel) != -1){
								wl_channel_list_5g[i] = _cur_channel;
								continue loop_auto;
							}
						}				
					}									
				}
				else if(bw_cap == "3"){	// 80 MHz
					document.getElementById('wl_nctrlsb_field').style.display = "";
					var _wl_channel = new Array();
					for(i=0;i<wl_channel_list_5g.length; i++){
						var _cur_channel = parseInt(wl_channel_list_5g[i]);
						for(j=0;j<wl1.channel_80m.length;j++){
							if(wl1.channel_80m[j].indexOf(_cur_channel) == 0){
								_wl_channel.push(_cur_channel+"/80");
							}
						}
					}

					wl_channel_list_5g = _wl_channel;									
				}
				else if(bw_cap == "2"){		// 40MHz
					document.getElementById('wl_nctrlsb_field').style.display = "";
					var _wl_channel = new Array();
					for(i=0;i<wl_channel_list_5g.length; i++){
						var _cur_channel = parseInt(wl_channel_list_5g[i]);
						for(j=0;j<wl1.channel_40m.length;j++){
							var _l = wl1.channel_40m[j].split("l");
							var _u = wl1.channel_40m[j].split("u");
							if(_l.length > 1){
								if(_l[0] == _cur_channel){
									_wl_channel.push(wlextchannel_fourty(_cur_channel));
								}
							}
							else{
								if(_u[0] == _cur_channel){
									_wl_channel.push(wlextchannel_fourty(_cur_channel));
								}
							}
						}	
					}

					wl_channel_list_5g = _wl_channel;						
				}
				else{		//20MHz
					document.getElementById('wl_nctrlsb_field').style.display = "none";
				}

				if(based_modelid == "RT-AC87U" && country == "EU"){		//for 5GHz DFS channel of RT-AC87U
					if(bw_cap == "1"){		// 20 MHz
						for(i=wl_channel_list_5g.length-1; i>=0; i--){		// remove channel between 52 ~ 140 DFS channel of RT-AC87U for EU, only under 20 MHz
							var _cur_channel = parseInt(wl_channel_list_5g[i]);
							if(_cur_channel >= 52 && _cur_channel <= 140){
								wl_channel_list_5g.splice(wl_channel_list_5g.getIndexByValue(_cur_channel),1);
							}
						}

						document.getElementById('dfs_checkbox').style.display = "none";
						document.form.acs_dfs.disabled = true;
					}
					else{
						if(bw_cap != bw_cap_ori){		//40 <-> 80 MHz switch each other
							document.getElementById('dfs_checkbox').style.display = "";
							document.form.acs_dfs.disabled = false;
						}
						else{		//switch back to origin bandwidth
							if(channel_ori == "0"){		//check control channel is auto to decide to show DFS checkbox
								document.getElementById('dfs_checkbox').style.display = "";
								document.form.acs_dfs.disabled = false;
							}
							else{
								document.getElementById('dfs_checkbox').style.display = "none";
								document.form.acs_dfs.disabled = true;
							}
						}			
					}			
				}
						
				/*add "Auto" into the option list*/		
				if(wl_channel_list_5g[0] != "0")	
					wl_channel_list_5g.splice(0,0,"0");
			
				add_options_x2(document.form.wl_nctrlsb, extend_channel, extend_channel_value, 1);   //construct extension channel
				chanspecs = wl_channel_list_5g;						
			}
		}
		else if (band == "0"){ // - 2.4 GHz				
			if(wl_channel_list_2g instanceof Array && wl_channel_list_2g != ["0"]){		//with wireless channel 2.4g hook or return ["0"]
				wl_channel_list_2g = JSON.parse('<% channel_list_2g(); %>');
				if(wl_channel_list_2g[0] != "0"){
					wl_channel_list_2g.splice(0,0,"0");
				}				
					
				if(cur.search('[ul]') != -1){
					cur_extend_channel = cur.slice(-1);			//current control channel
					cur_control_channel = cur.split(cur_extend_channel)[0];	//current extension channel direction
				}
				else{
					cur_control_channel = cur;
				}				
						
				if(bw_cap == "2" || bw_cap == "0") { 	// -- [40 MHz]  | [20/40 MHz]				
					document.getElementById('wl_nctrlsb_field').style.display = "";
					if(cur_control_channel == 0){
						extend_channel = ["<#Auto#>"];
						extend_channel_value = ["1"];
						add_options_x2(document.form.wl_nctrlsb, extend_channel, extend_channel_value, 1);	
					}
					else if(cur_control_channel >= 1 && cur_control_channel <= 4){
						extend_channel = ["<#WLANConfig11b_EChannelAbove#>"];
						add_options_x2(document.form.wl_nctrlsb, extend_channel, "l");							
					}
					else if(wl_channel_list_2g.length == 12){    // 1 ~ 11
						if(cur_control_channel >= 5 && cur_control_channel <= 7){
							extend_channel = ["<#WLANConfig11b_EChannelAbove#>", "<#WLANConfig11b_EChannelBelow#>"];
							extend_channel_value = ["l", "u"];
							add_options_x2(document.form.wl_nctrlsb, extend_channel, extend_channel_value, cur_extend_channel);							
						}
						else if(cur_control_channel >= 8 && cur_control_channel <= 11){
							extend_channel = ["<#WLANConfig11b_EChannelBelow#>"];
							extend_channel_value = ["u"];
							add_options_x2(document.form.wl_nctrlsb, extend_channel, extend_channel_value, cur_extend_channel);								
						}
					}
					else{		// 1 ~ 13
						if(cur_control_channel >= 5 && cur_control_channel <= 9){
							extend_channel = ["<#WLANConfig11b_EChannelAbove#>", "<#WLANConfig11b_EChannelBelow#>"];
							extend_channel_value = ["l", "u"];
							add_options_x2(document.form.wl_nctrlsb, extend_channel, extend_channel_value, cur_extend_channel);							
						}
						else if(cur_control_channel >= 10 && cur_control_channel <= 13){
							extend_channel = ["<#WLANConfig11b_EChannelBelow#>"];
							extend_channel_value = ["u"];
							add_options_x2(document.form.wl_nctrlsb, extend_channel, extend_channel_value, cur_extend_channel);								
						}			
					}
				}else{		// -- 20 MHz
					cur_control_channel = cur;
					document.getElementById('wl_nctrlsb_field').style.display = "none";
				}	
						
				chanspecs = wl_channel_list_2g;
			}		
		}
		else if(band == "2"){	// 5 GHz-2 - high band
			wl_channel_list_5g_2 = JSON.parse('<% channel_list_5g_2(); %>');					
			extend_channel = ["<#Auto#>"];		 // for 5GHz, extension channel always displays Auto
			extend_channel_value = [""];

			if(bw_cap == "0"){	// 20/40/80 MHz (auto)
				document.getElementById('wl_nctrlsb_field').style.display = "";

	 loop_auto: for(i=0; i<wl_channel_list_5g_2.length; i++){
					var _cur_channel = parseInt(wl_channel_list_5g_2[i]);
					if(band5g_11ac_support && document.form.wl_nmode_x.value != 1){
						for(j=0;j<wl2.channel_80m.length;j++){
							if(wl2.channel_80m[j].indexOf(_cur_channel) != -1){
								wl_channel_list_5g_2[i] = _cur_channel + "/80";
								continue loop_auto;
							}
						}
					}

					for(j=0;j<wl2.channel_40m.length;j++){
						if(wl2.channel_40m[j].indexOf(_cur_channel) != -1){
							wl_channel_list_5g_2[i] = wlextchannel_fourty(_cur_channel);
							continue loop_auto;
						}
					}

					for(j=0;j<wl2.channel_20m.length;j++){
						if(wl2.channel_20m[j].indexOf(_cur_channel) != -1){
							wl_channel_list_5g_2[i] = _cur_channel;
							continue loop_auto;
						}
					}				
				}								
			}
			else if(bw_cap == "3"){	// 80 MHz
				document.getElementById('wl_nctrlsb_field').style.display = "";
				var _wl_channel = new Array();
				for(i=0;i<wl_channel_list_5g_2.length; i++){
					var _cur_channel = parseInt(wl_channel_list_5g_2[i]);
					
					for(j=0;j<wl2.channel_80m.length;j++){
						if(wl2.channel_80m[j].indexOf(_cur_channel) != -1){
							_wl_channel.push(_cur_channel+"/80");
						}
					}
				}

				wl_channel_list_5g_2 = _wl_channel;								
			}
			else if(bw_cap == "2"){		// 40 MHz
				document.getElementById('wl_nctrlsb_field').style.display = "";
				var _wl_channel = new Array();
				for(i=0;i<wl_channel_list_5g_2.length; i++){
					var _cur_channel = parseInt(wl_channel_list_5g_2[i]);
					for(j=0;j<wl2.channel_40m.length;j++){
						var _l = wl2.channel_40m[j].split("l");
						var _u = wl2.channel_40m[j].split("u");
						if(_l.length > 1){
							if(_l[0] == _cur_channel){
								_wl_channel.push(wlextchannel_fourty(_cur_channel));
							}
						}
						else{
							if(_u[0] == _cur_channel){
								_wl_channel.push(wlextchannel_fourty(_cur_channel));
							}
						}
					}
				}

				wl_channel_list_5g_2 = _wl_channel;	
			}
			else{		//20MHz
				document.getElementById('wl_nctrlsb_field').style.display = "none";					
			}
			
			/*add "Auto" into the option list*/	
			if(wl_channel_list_5g_2[0] != "0")
				wl_channel_list_5g_2.splice(0,0,"0");
			
			add_options_x2(document.form.wl_nctrlsb, extend_channel, extend_channel_value, 1);   //construct extension channel
			chanspecs = wl_channel_list_5g_2;				
		}	//end 5GHz-2 - high band
	} 
	else { // b/g mode 
		chanspecs = new Array(0);
	}

	/* Reconstruct channel array from new chanspecs */
	document.form.wl_channel.length = chanspecs.length;
	if(band == 1 || band == 2){
		for (var i in chanspecs){
			if (chanspecs[i] == 0)
				document.form.wl_channel[i] = new Option("<#Auto#>", chanspecs[i]);
			else
				document.form.wl_channel[i] = new Option(chanspecs[i].toString().replace("/80", "").replace("u", "").replace("l", ""), chanspecs[i]);
			document.form.wl_channel[i].value = chanspecs[i];
			if (chanspecs[i] == cur && bw_cap == '<% nvram_get("wl_bw"); %>'){
				document.form.wl_channel[i].selected = true;
				sel = 1;
			}
		}
		if (sel == 0 && document.form.wl_channel.length > 0)
			document.form.wl_channel[0].selected = true;
	}
	else{
		for(i=0;i< chanspecs.length;i++){
			if(i == 0)
				chanspecs_string[i] = "<#Auto#>";
			else
				chanspecs_string[i] = chanspecs[i];
		}

		add_options_x2(document.form.wl_channel, chanspecs_string, chanspecs, cur_control_channel);
	}
}

function wlextchannel_fourty(v){
	var tmp_wl_channel_5g = "";
	if(v > 144){
		tmp_wl_channel_5g = v - 1;
		if(tmp_wl_channel_5g%8 == 0)
			v = v + "u";
		else		
			v = v + "l";
	}else{
		if(v%8 == 0)
			v = v + "u";
		else		
			v = v + "l";											
	}
	
	return v;
}

var wl1_dfs = '<% nvram_get("wl1_dfs"); %>';
function change_channel(obj){
	var extend_channel = new Array();
	var extend_channel_value = new Array();
	var selected_channel = obj.value;
	var channel_length =obj.length;
	var band = document.form.wl_unit.value;
	if(document.form.wl_bw.value != 1){   // 20/40 MHz or 40MHz
		if(channel_length == 12){    // 1 ~ 11
			if(selected_channel >= 1 && selected_channel <= 4){
				extend_channel = ["<#WLANConfig11b_EChannelAbove#>"];
				extend_channel_value = ["l"];
				add_options_x2(document.form.wl_nctrlsb, extend_channel, extend_channel_value);				
			}
			else if(selected_channel >= 5 && selected_channel <= 7){
				extend_channel = ["<#WLANConfig11b_EChannelAbove#>", "<#WLANConfig11b_EChannelBelow#>"];
				extend_channel_value = ["l", "u"];
				add_options_x2(document.form.wl_nctrlsb, extend_channel, extend_channel_value);							
			}
			else if(selected_channel >= 8 && selected_channel <= 11){
				extend_channel = ["<#WLANConfig11b_EChannelBelow#>"];
				extend_channel_value = ["u"];
				add_options_x2(document.form.wl_nctrlsb, extend_channel, extend_channel_value);								
			}
			else{				//for 0: Auto
				extend_channel = ["<#Auto#>"];
				extend_channel_value = [""];
				add_options_x2(document.form.wl_nctrlsb, extend_channel, extend_channel_value);
			}
		}
		else{		// 1 ~ 13
			if(selected_channel >= 1 && selected_channel <= 4){
				extend_channel = ["<#WLANConfig11b_EChannelAbove#>"];
				extend_channel_value = ["l"];
				add_options_x2(document.form.wl_nctrlsb, extend_channel, extend_channel_value);							
			}
			else if(selected_channel >= 5 && selected_channel <= 9){
				extend_channel = ["<#WLANConfig11b_EChannelAbove#>", "<#WLANConfig11b_EChannelBelow#>"];
				extend_channel_value = ["l", "u"];
				add_options_x2(document.form.wl_nctrlsb, extend_channel, extend_channel_value);							
			}
			else if(selected_channel >= 10 && selected_channel <= 13){
				extend_channel = ["<#WLANConfig11b_EChannelBelow#>"];
				extend_channel_value = ["u"];
				add_options_x2(document.form.wl_nctrlsb, extend_channel, extend_channel_value);								
			}
			else{				//for 0: Auto
				extend_channel = ["<#Auto#>"];
				extend_channel_value = [""];
				add_options_x2(document.form.wl_nctrlsb, extend_channel, extend_channel_value);
			}
		}
	}
	
	if(band == 1){
		if(country == "EU"){		// for DFS channel
			if(based_modelid == "RT-AC68U" || based_modelid == "RT-AC68A" || based_modelid == "4G-AC68U" || based_modelid == "DSL-AC68U"
			|| (based_modelid == "RT-AC66U" && wl1_dfs == "1")
			|| based_modelid == "RT-N66U"){
				if(document.form.wl_channel.value  == 0){
					document.getElementById('dfs_checkbox').style.display = "";
					document.form.acs_dfs.disabled = false;
				}	
				else{
					document.getElementById('dfs_checkbox').style.display = "none";
					document.form.acs_dfs.disabled = true;
				}
			}
			else if(based_modelid == "RT-AC87U"){
				if(document.form.wl_channel.value  == "0"){
					if(document.form.wl_bw.value == "1"){
						document.getElementById('dfs_checkbox').style.display = "none";
						document.form.acs_dfs.disabled = true;
					}
					else{
						document.getElementById('dfs_checkbox').style.display = "";
						document.form.acs_dfs.disabled = false;
					}
				}
				else{
					document.getElementById('dfs_checkbox').style.display = "none";
					document.form.acs_dfs.disabled = true;
				}				
			}
		}
		else if(country == "US" && dfs_US_support){
			if(document.form.wl_channel.value  == 0){
				document.getElementById('dfs_checkbox').style.display = "";
				document.form.acs_dfs.disabled = false;
			}	
			else{
				document.getElementById('dfs_checkbox').style.display = "none";
				document.form.acs_dfs.disabled = true;
			}
		}
		else if(country == "US" || country == "SG"){			//for acs band1 channel
			if(based_modelid == "RT-AC68U" || based_modelid == "RT-AC68A" || based_modelid == "4G-AC68U" || based_modelid == "DSL-AC68U"
			|| based_modelid == "RT-AC56U" || based_modelid == "RT-AC56S"
			|| based_modelid == "RT-AC66U"
			|| based_modelid == "RT-N66U"
			|| based_modelid == "RT-AC53U"){
				if(document.form.wl_channel.value  == 0){
					document.getElementById('acs_band1_checkbox').style.display = "";
					document.form.acs_band1.disabled = false;
				}	
				else{
					document.getElementById('acs_band1_checkbox').style.display = "none";
					document.form.acs_band1.disabled = true;
				}
			}
		}
	}
	else if(band == 0){
		if(country == "EU" || country == "JP" || country == "SG" || country == "CN" || country == "UA" || country == "KR"){
			if(!Qcawifi_support && !Rawifi_support){
				if(document.form.wl_channel.value  == '0'){
					document.getElementById('acs_ch13_checkbox').style.display = "";
					document.form.acs_ch13.disabled = false;					
				}
				else{
					document.getElementById('acs_ch13_checkbox').style.display = "none";
					document.form.acs_ch13.disabled = true;							
				}
			}
		}
	}	
}
