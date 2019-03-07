#!/bin/bash

function show_help() {
    echo -e "Usage: $0 <platform> <codec_module>\n"\
      "  Supported platforms: byt, apl, cnl, icl or whl."
    exit 1
}

function probe_module() {
    local module="$1"
    echo "modprobing $module"
    echo $sudopwd | sudo -S modprobe $module
}

declare -fx probe_module

function load_modules(){
    if [ $# -lt 2 ] || [ "$1" == "-h" ] || [ "$1" == "--help" ] ; then
        show_help $0
    fi

    local platform=$1
    local codec_module=$2

    if [ ! -z $codec_module ]; then
        probe_module $codec_module
    fi

    case "$platform" in
        byt)
            probe_module sof_acpi_dev
            ;;
        apl|icl)
            probe_module sof_pci_dev
            ;;
        cnl)
            #need to test
            probe_module sof_pci_dev
            ;;
        whl)
            #need to test
            probe_module sof_pci_dev
            ;;
        *)
            echo -e "Invalid platform: $platform"
            ;;
    esac
}

declare -fx load_modules
