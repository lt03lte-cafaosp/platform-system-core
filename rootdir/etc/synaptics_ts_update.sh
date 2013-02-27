#!/system/bin/sh

firmware_ver=/etc/firmware/synaptics_fw_version
for path in /sys/class/input/input*
do
    name=`cat "$path/name"`
    case "$name" in
        synaptics_dsx_i2c)
            custom_id=`cat "$path"/custom_config_id`
            chip=`cat "$path"/chip`
            version=`cat "$firmware_ver"_"$chip"`
            if [ "$custom_id" != "$version" ]; then
                echo 1 > "$path/doreflash"
            fi
            break;
            ;;
    esac
done

