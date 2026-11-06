/* Regression tests for Android shared-storage filename restrictions. */
#include "config.h"
#include "libmtp.h"
#include "unicode.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int check(LIBMTP_mtpdevice_t *device, const char *input,
                 const char *expected)
{
  char *name = strdup(input);
  int failed;

  sanitize_android_filename(device, name);
  failed = strcmp(name, expected) != 0;
  if (failed)
    fprintf(stderr, "Filename mismatch: expected [%s], got [%s]\n", expected, name);
  free(name);
  return failed;
}

int main(void)
{
  LIBMTP_mtpdevice_t device = {0};
  LIBMTP_device_extension_t android = {0};
  LIBMTP_device_extension_t other = {0};
  int failed = 0;

  android.name = "android.com";
  other.name = "microsoft.com";
  other.next = &android;
  device.extensions = &other;

  failed |= check(&device,
      "333. Lorem Ipsum: Dolor Sit Amet.mp3",
      "333. Lorem Ipsum_ Dolor Sit Amet.mp3");
  failed |= check(&device, "Lorem's ipsum: café – 例.mp3",
                          "Lorem's ipsum_ café – 例.mp3");
  failed |= check(&device, "\"*/:<>?\\|", "_________");
  failed |= check(&device, "\001\037\177.mp3", "___.mp3");
  failed |= check(&device, "normal file's name.mp3", "normal file's name.mp3");
  failed |= check(&device, ".hidden", ".hidden");
  failed |= check(&device, "", "");
  sanitize_android_filename(&device, NULL);

  /* Other responders must retain their own filename semantics. */
  other.next = NULL;
  failed |= check(&device, "Lorem's ipsum: café.mp3", "Lorem's ipsum: café.mp3");
  device.extensions = NULL;
  failed |= check(&device, "\"*/:<>?\\|", "\"*/:<>?\\|");
  return failed ? EXIT_FAILURE : EXIT_SUCCESS;
}
