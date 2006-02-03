#!/bin/sh
set -e

# Refresh GNU autotools toolchain.
libtoolize --copy --force
aclocal
autoheader
automake --add-missing
autoconf

# Autoupdate config.sub and config.guess 
# from GNU CVS
WGET=`which wget`
if [ "x$WGET" != "x" ]; then
    echo "Autoupdate config.sub and config.guess (y/n)?"
    read IN
    if [ "$IN" = "y" ] || [ "$IN" = "Y" ]; then
	wget -O tmpfile http://savannah.gnu.org/cgi-bin/viewcvs/*checkout*/config/config/config.guess
	mv tmpfile config.guess
	wget -O tmpfile http://savannah.gnu.org/cgi-bin/viewcvs/*checkout*/config/config/config.sub
	mv tmpfile config.sub
    fi
else
    echo "Could not autoupdate config.sub and config.guess"
fi
