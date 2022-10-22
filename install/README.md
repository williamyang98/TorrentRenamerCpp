# Install instructions
## Windows
Just create a start menu shortcut from the context menu when you right click the binary.

## Linux
For linux installs using the GNOME desktop environment, you can't install start menu shortcuts in an easy way. Copy the *.desktop file entry to **~/.local/share/applications/\*.desktop**.

On linux you need to specify the LD_LIBRARY_PATH variable so that it loads the dynamic libraries. The bash script **run.sh** does this for us.

Copy the binary and the run.sh script to your desired install location, and edit the .desktop file to point to it.
