bin_PROGRAMS=connect detect tracks files hotplug \
	folders trexist playlists getplaylist \
	format albumart

connect_SOURCES=connect.c sendfile.c sendtr.c pathutils.c common.h
detect_SOURCES=detect.c common.h
tracks_SOURCES=tracks.c common.h
files_SOURCES=files.c common.h
hotplug_SOURCES=hotplug.c common.h
folders_SOURCES=folders.c common.h
trexist_SOURCES=trexist.c common.h
playlists_SOURCES=playlists.c common.h
getplaylist_SOURCES=getplaylist.c common.h
format_SOURCES=format.c common.h
albumart_SOURCES=albumart.c common.h

AM_CFLAGS=-I$(top_builddir)/src
LDADD=../src/libmtp.la
EXTRA_DIST=evolution-sync.sh
