#!/bin/bash

BAG_XML_DUMP_PATH="lvbag-extract-nl.zip"

help_info()
{
   # Display Help
   echo "Download and unzip the latest BAG-xml dump."
   echo "If no arguments are provided, the BAG-xml is only downloaded when no dump exists,"
   echo "or when a new BAG-xml dump is available."
   echo
   echo "Syntax: get-bag-xml.sh [-g|f|d]"
   echo "options:"
   echo "h     Print this help."
   echo "f     Always re-download the BAG-xml"
   echo "d     Never download BAG-xml, just unzip it"
   echo
}

download_latest_bag_dump()
{
    wget https://service.pdok.nl/lv/bag/atom/downloads/lvbag-extract-nl.zip -O "$BAG_XML_DUMP_PATH"
}

download_latest_bag_when_no_dump_is_found()
{
    if [ ! -f "$BAG_XML_DUMP_PATH" ]; then
        download_latest_bag_dump
    fi
}

download_if_new_bag_dump_is_expected()
{
    # A new BAG dump is provided each on the 8th of each month,
    # so only re-download it when we expect a new dump to be available.
    # Will always download if no current BAG-dump is available.
    cur_month=$(date "+%m")
    cur_day=$(date "+%d")

    if [ -f "$BAG_XML_DUMP_PATH" ]; then
        dump_month=$(date -r "$BAG_XML_DUMP_PATH" "+%m")
        dump_day=$(date -r "$BAG_XML_DUMP_PATH" "+%d")
    else
        download_latest_bag_dump
        return
    fi

    if (( cur_month > dump_month && cur_day >= 8)); then
        echo "Expecting a new dump to be available, downloading..."
        download_latest_bag_dump
    elif (( cur_month == dump_month && cur_day > dump_day && cur_day >= 8)); then
        echo "Expecting a new dump to be available, downloading..."
        download_latest_bag_dump
    else
        echo "No new BAG is available, not downloading again."
    fi
}

unzip_bag_dump()
{
    rm -rf bag
    mkdir bag
    cd bag || exit
    unzip ../lvbag-extract-nl.zip
    echo 9999{WPL,OPR,NUM,VBO,LIG,STA,PND}*.zip | xargs -n1 -P8 unzip
}

while getopts ":hfd:" option; do
   case $option in
      h) # display Help
        help_info
        exit;;
      f) # Always re-download the BAG xml dump
        download_latest_bag_dump
        unzip_bag_dump
        exit;;
      d) # Dry-run mode: Never re-download the BAG xml dump, just unzip existing file.
        unzip_bag_dump
        exit;;
     \?) # Invalid option
        echo "Error: Invalid option"
        exit;;
   esac
done

# When no option is given, only download when needed
download_if_new_bag_dump_is_expected
unzip_bag_dump