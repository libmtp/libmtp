bin_PROGRAMS=connect detect tracks files hotplug \
	folders trexist playlists getplaylist \
	format albumart albums newplaylist

connect_SOURCES=connect.c delfile.c getfile.c newfolder.c sendfile.c sendtr.c pathutils.c common.h
detect_SOURCES=detect.c common.h
tracks_SOURCES=tracks.c common.h
files_SOURCES=files.c common.h
hotplug_SOURCES=hotplug.c common.h
folders_SOURCES=folders.c common.h
trexist_SOURCES=trexist.c common.h
playlists_SOURCES=playlists.c common.h
getplaylist_SOURCES=getplaylist.c common.h
newplaylist_SOURCES=newplaylist.c common.h
format_SOURCES=format.c common.h
albumart_SOURCES=albumart.c common.h
albums_SOURCES=albums.c common.h

AM_CFLAGS=-I$(top_builddir)/src
LDADD=../src/libmtp.la
EXTRA_DIST=evolution-sync.sh

install-exec-hook:
	ln -f -s $(DESTDIR)$(bindir)/mtp-connect $(DESTDIR)$(bindir)/mtp-delfile
	ln -f -s $(DESTDIR)$(bindir)/mtp-connect $(DESTDIR)$(bindir)/mtp-getfile
	ln -f -s $(DESTDIR)$(bindir)/mtp-connect $(DESTDIR)$(bindir)/mtp-newfolder
	ln -f -s $(DESTDIR)$(bindir)/mtp-connect $(DESTDIR)$(bindir)/mtp-sendfile
	ln -f -s $(DESTDIR)$(bindir)/mtp-connect $(DESTDIR)$(bindir)/mtp-sendtr
	
