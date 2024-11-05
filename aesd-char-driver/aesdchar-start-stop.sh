#!/bin/sh

case "$1" in 
    start)
        echo "Loading aesdchar device"
        aesdchar_load
        ;;
    stop)
        echo "Unloading aesdchar device"
        aesdchar_unload
        ;;
    *)
        echo "Usage: $0 {start|stop}"
    exit 1
esac 

exit 0

