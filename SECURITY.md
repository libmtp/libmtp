# Security overview

## General

libmtp is a software library to allow accessing MTP based USB devices
(media players in the past, now the largest set is Android phones and
also Garmin devices), allowing various file system operations (list,
download, upload, delete).

Callers of the library can be assumed trusted, also input coming into
the library via API calls is considered trusted.

Data coming from USB devices is considered untrusted.

## Attack Surface

The primary attack scenario is an automated access and download from
MTP devices on a public computer, where people can plug in USB devices
in an unattended fashion.

Attack impacts are achieving control over this computer, or blocking its use.

## Bugs considered security issues

(Mostly for CVE assignments rules.)

Triggering memory corruption when processing USB data is considered in scope.

Triggering endless loops when processing USB data is considered in scope. (would block kiosk style operation)

## Bugs not considered security issues

Information disclosure is not a relevant attack scenario.

## Bugreports

Bugreports can be filed as github issues.

If you want to report an embargoed security bug report, reach out to marcus@jet.franken.de.
