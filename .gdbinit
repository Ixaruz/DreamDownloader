# .gdbinit - Nintendo Switch homebrew debugging automation

define switch-connect
    if $argc != 2
        printf "Usage: switch-connect <IP:PORT> <NRO>\n"
        printf "Example: switch-connect 192.168.1.100:22225 build/main.release/DreamDownloader.nro\n"
    else
        printf "Connecting to Switch at %s...\n", "$arg0"

        # Connect to the switch
        target extended-remote $arg0

        # Get hbloader process ID using pipe command
        printf "Getting hbloader process ID...\n"
        pipe info os processes | grep -E "^[0-9]+\s*hbloader" | awk '{print $1}' > .hblPid

        # Read the PID and create attach command
        shell if [ -s .hblPid ]; then echo "attach " $(cat .hblPid) > .gdbtmp; echo "set \$hblPid = " $(cat .hblPid) >> .gdbtmp; else echo "set \$hblPid = 0" > .gdbtmp; fi

        source .gdbtmp

        if $hblPid == 0
            printf "No hbloader process found!\n"
            info os processes
        else
            printf "Attached to hbloader process %d\n", $hblPid

            printf "Uploading homebrew via nxlink...\n"
            # Extract IP from the IP:PORT argument and upload the NRO
            shell echo "$arg0" | cut -d':' -f1 > .switchip
            shell SWITCH_IP=$(cat .switchip) && nxlink -sa $SWITCH_IP "$arg1" &
            shell rm -f .switchip
            printf "nxlink started in background, waiting for breakpoint...\n"
            printf "Continuing execution...\n"
            continue



            printf "Breakpoint hit! Getting module info...\n"

            # Get module information and extract the ELF address using the basename
            pipe monitor get info | grep $(basename $arg1 | cut -d"." -f1)".elf$" | awk '{print $1}' >> .elfAddr

            # Create the add-symbol-file command using the full ELF path
            shell if [ -s .elfAddr ]; then ELFPATH=$(echo $(dirname $arg1)"/"$(basename $arg1 | cut -d"." -f1)".elf") && echo "add-symbol-file $ELFPATH " $(cat .elfAddr) > .gdbtmp2; echo "set \$elfAddr = " $(cat .elfAddr) >> .gdbtmp2; else echo "set \$elfAddr = 0" > .gdbtmp2; fi

            source .gdbtmp2

            if $elfAddr == 0
                printf "Could not find %s.elf address!\n", "$basename"
                monitor get info
                printf "Please manually run: add-symbol-file %s <address>\n", "$elfpath"
            else
                printf "Symbols loaded successfully at address %p!\n", $elfAddr
            end
        end

        # Cleanup temp files
        shell rm -f .hblPid .gdbtmp .elfAddr .gdbtmp2
    end
end

# Set up some useful defaults
set confirm off
set pagination off
set print pretty on
