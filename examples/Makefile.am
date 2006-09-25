bin_PROGRAMS=detect tracks files getfile sendtr delfile hotplug \
	folders newfolder trexist sendfile playlists getplaylist \
	format

detect_SOURCES=detect.c common.h
tracks_SOURCES=tracks.c common.h
files_SOURCES=files.c common.h
getfile_SOURCES=getfile.c common.h
sendtr_SOURCES=sendtr.c common.h
sendfile_SOURCES=sendfile.c common.h
delfile_SOURCES=delfile.c common.h
hotplug_SOURCES=hotplug.c common.h
folders_SOURCES=folders.c common.h
newfolder_SOURCES=newfolder.c common.h
trexist_SOURCES=trexist.c common.h
playlists_SOURCES=playlists.c common.h
getplaylist_SOURCES=getplaylist.c common.h
format_SOURCES=format.c common.h

AM_CFLAGS=-I$(top_builddir)/src
LDADD=../src/libmtp.la
EXTRA_DIST=evolution-sync.sh
