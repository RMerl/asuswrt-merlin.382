#!/bin/sh

wget_options="-q -t 2 -T 30"

nvram set cfg_note=0
get_preferred_lang=$(nvram get preferred_lang)
LANG="$get_preferred_lang"
forsq=$(nvram get apps_sq)

if [ $# != 2 ]; then
	echo "---- no model name or version ----" >> /tmp/webs_note.log
	nvram set cfg_note=2
	exit 0
fi

new_firm=$(echo "$2" | sed s/'\.'/_/4 | sed s/'\.'//g;)
echo "---- $new_firm ----" >> /tmp/webs_note.log

if [ "$(echo "$2" | cut -d "_" -f 3)" = "" ]; then
	echo "Asus FW" >> /tmp/webs_note.log
	releasenote_file=$1_"$new_firm"_"$LANG"_note.zip
	releasenote_file_US=$1_"$new_firm"_US_note.zip
	fwsite="https://dlcdnets.asus.com/pub/ASUS/wireless/ASUSWRT"
	fwsiteSQ=$fwsite
else
	echo "Merlin FW" >> /tmp/webs_note.log
	releasenote_file=$(nvram get webs_state_info_am)_note.txt
	releasenote_file_US=$releasenote_file
	fwsite="https://fwupdate.asuswrt-merlin.net"
	fwsiteSQ=$fwsite"/test"
fi
	releasenote_path="/tmp/release_note.txt"

if [ "$forsq" = "1" ]; then
	echo "---- download SQ release note $fwsiteSQ/$releasenote_file ----" >> /tmp/webs_note.log
	if ! wget "$wget_options" $fwsiteSQ/"$releasenote_file" -O $releasenote_path; then
		wget "$wget_options" $fwsiteSQ/"$releasenote_file_US" -O $releasenote_path
	else
	        wget "$wget_options" $fwsiteSQ/"$releasenote_file" -O $releasenote_path
	fi
	echo "---- $fwsiteSQ/$releasenote_file ----" >> /tmp/webs_note.log
else
	echo "---- download real release note for $1 ----" >> /tmp/webs_note.log
	if ! wget "$wget_options" $fwsite/"$releasenote_file" -O $releasenote_path; then
		wget "$wget_options" $fwsite/"$releasenote_file_US" -O $releasenote_path
	else
	        wget "$wget_options" $fwsite/"$releasenote_file" -O $releasenote_path
	fi
	echo "---- $fwsite/$releasenote_file ----" >> /tmp/webs_note.log
fi

exitcode="$?"

if [ $exitcode != "0" ] || [ ! -s $releasenote_path ]; then
	echo "---- download release note failed ----" >> /tmp/webs_note.log
	nvram set cfg_note=3
	exit 0
fi
nvram set cfg_note=1
