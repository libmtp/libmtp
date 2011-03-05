bin_PROGRAMS=mtp-connect mtp-detect mtp-tracks mtp-files \
	mtp-folders mtp-trexist mtp-playlists mtp-getplaylist \
	mtp-format mtp-albumart mtp-albums mtp-newplaylist mtp-emptyfolders \
	mtp-thumb mtp-reset mtp-filetree

mtp_connect_SOURCES=connect.c connect.h delfile.c getfile.c newfolder.c \
	sendfile.c sendtr.c pathutils.c pathutils.h \
	util.c util.h common.h
mtp_detect_SOURCES=detect.c util.c util.h common.h
mtp_tracks_SOURCES=tracks.c util.c util.h common.h
mtp_files_SOURCES=files.c util.c util.h common.h
mtp_folders_SOURCES=folders.c util.c util.h common.h
mtp_trexist_SOURCES=trexist.c util.c util.h common.h
mtp_playlists_SOURCES=playlists.c util.c util.h common.h
mtp_getplaylist_SOURCES=getplaylist.c util.c util.h common.h
mtp_newplaylist_SOURCES=newplaylist.c util.c util.h common.h
mtp_format_SOURCES=format.c util.c util.h common.h
mtp_albumart_SOURCES=albumart.c util.c util.h common.h
mtp_albums_SOURCES=albums.c util.c util.h common.h
mtp_emptyfolders_SOURCES=emptyfolders.c util.c util.h common.h
mtp_thumb_SOURCES=thumb.c util.c util.h common.h
mtp_reset_SOURCES=reset.c util.c util.h common.h
mtp_filetree_SOURCES=filetree.c util.c util.h common.h

AM_CPPFLAGS=-I$(top_builddir)/src
LDADD=../src/libmtp.la
EXTRA_DIST=evolution-sync.sh

install-exec-hook:
	ln -f -s mtp-connect$(EXEEXT) $(DESTDIR)$(bindir)/mtp-delfile$(EXEEXT)
	ln -f -s mtp-connect$(EXEEXT) $(DESTDIR)$(bindir)/mtp-getfile$(EXEEXT)
	ln -f -s mtp-connect$(EXEEXT) $(DESTDIR)$(bindir)/mtp-newfolder$(EXEEXT)
	ln -f -s mtp-connect$(EXEEXT) $(DESTDIR)$(bindir)/mtp-sendfile$(EXEEXT)
	ln -f -s mtp-connect$(EXEEXT) $(DESTDIR)$(bindir)/mtp-sendtr$(EXEEXT)
