# Release checklist

# GIT work

- make check
- change configure.ac to the new version
- commit
- tag this commit with libmtp-1-1-x and v1.1.x
- git push --tags  		to sourceforge and
- git push --tags github	to github
- make dist
- gpg sign created tarball via gpg --detach-sign -a libmtp-1.1.x.tar.gz
- create a README with the git shortlog libmtp-1-1-y..libmtp-1-1-x and perhaps some highlevel words

## SF work

- create a new directory 1.1.x
- upload the tar.gz, tar.gz.asc and README to this Folder
- select the tar.gz as new default download for sF

- Go to Tracker -> Bugs -> Edit Milestones and add the new release
- Go to Tracker -> Support Requests -> Edit Milestones and add the new release

## Github work

- Go to the tags -> release 
- create a new release from the libmtp-1-1-x tag
- Put in README like above
- upload also .tar.gz and .tar.gz.asc so people have a verifiable tarball.

## Announce

- announce on https://freshcode.club/
