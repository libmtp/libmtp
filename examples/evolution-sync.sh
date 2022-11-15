#!/bin/bash
#
# Copyright (C) 2006 Linus Walleij <triad@df.lth.se>
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, write to the
# Free Software Foundation, Inc., 59 Temple Place - Suite 330,
# Boston, MA 02111-1307, USA.

# Example evolution synchronization script by Nicolas Tetreault,
# modified by Linus Walleij.

# Define target files
SYNC_HOME=$HOME/MTP_device_sync

# Define tool locations
SENDFILE=`which mtp-sendfile`
# SENDFILE="$HOME/comp-apps/bin/sendfile"
#EADDEXP=`which evolution-addressbook-export`
# This is the location in Fedora Core 5:
EADDEXP="/usr/libexec/evolution/2.6/evolution-addressbook-export"

# You need to change the name of the files
# that contains the calendar and contacts on your device. 
# You can find out by  starting Gnomad2, choose the data transfer 
# tab, sort by size (it should be small files, extension .ics and .vcf)
# On my Zen Microphoto, the calendar and contacts files are called
# 6651416 with the ics and vcf extensions, respectively.
CALENDAR_FILE="6651416.ics"
CONTACTS_FILE="6651416.vcf"

# The evolution address book. To list your addressbooks, type:
# evolution-addressbook-export -l
# the output for me:
# "file:///home/nt271/.evolution/addressbook/local/system
# ","Personal",26
# "file:///home/nt271/.evolution/addressbook/local/1158600180.5386.0@sierra"
# ,"MicroPhoto",24
# I only want the Microphoto addressbook and the output will be
# $SYNC_HOME/contacts/Evolution_contacts.vcf
EVOLUTION_CONTACTS="file:///home/linus/.evolution/addressbook/local/system"

# Check for sync dir, create it if needed

if test -d $SYNC_HOME ; then
    echo "$SYNC_HOME exists, OK."
else
    echo "$SYNC_HOME must first be created..."
    mkdir $SYNC_HOME
    # This is a working dir for contact merging, you can put
    # in some extra .vcf files here as well if you like.
    mkdir $SYNC_HOME/contacts
    # Here you can place some extra calendars to be sync:ed, you
    # can put in some extra .ics files of any kind here.
    mkdir $SYNC_HOME/calendars
fi

# Check for prerequisites

if test -f $EADDEXP ; then
    echo "evolution-addressbook-export present in $EADDEXP, OK."
else
    echo "Cannot locate evolution-addressbook-export!!"
    exit 0
fi


# Next line merges all of your tasklist, your personal calendar, 
# and then any saved to disk calendar you have placed in
# $SYNC_HOME/calendars

cat $HOME/.evolution/tasks/local/system/tasks.ics \
    $HOME/.evolution/calendar/local/system/calendar.ics \
    $SYNC_HOME/calendars/*.ics > $SYNC_HOME/$CALENDAR_FILE

# Use evolution-addressbook-export (installed with Evolution) to
# export your contacts to vcard.

$EADDEXP --format=vcard \
    --output=$SYNC_HOME/contacts/Evolution_contacts.vcf \
    $EVOLUTION_CONTACTS

# Repeat for each addressbook you want to upload.

# The next command will then merge all the contact lists

cat $SYNC_HOME/contacts/*.vcf > $SYNC_HOME/$CONTACTS_FILE

# The calendar and contacts files now need to be converted from unix
# to DOS linefeeds (CR+LF instead of just LF)

unix2dos $SYNC_HOME/$CALENDAR_FILE $SYNC_HOME/$CONTACTS_FILE

# You can now upload the ics and vcf files to you My Organizer folder
# on your device. Change the path to your sendfile command.
# Sending the vcf file is only supported in CVS version at this time

$SENDFILE -f "My Organizer" -t ics $SYNC_HOME/$CALENDAR_FILE
$SENDFILE -f "My Organizer" -t vcf $SYNC_HOME/$CONTACTS_FILE

