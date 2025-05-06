/* testpci.c - Test if PCI exists by ID.  */
/*
 *  GRUB  --  GRand Unified Bootloader
 *  Copyright (C) 2025  Free Software Foundation, Inc.
 *
 *  GRUB is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  GRUB is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with GRUB.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <grub/dl.h>
#include <grub/extcmd.h>
#include <grub/mm.h>
#include <grub/file.h>
#include <grub/normal.h>
#include <grub/pci.h>

GRUB_MOD_LICENSE ("GPLv3+");

static const struct grub_arg_option options[] = {
  {"file", 0, 0, "read device list from file", "FILE", ARG_TYPE_STRING},
  {0, 0, 0, 0, 0, 0}
};

struct grub_testpci_devlist {
  char** devices;
  int n_devices;
  int s_devices;
  bool found;
};

static int
grub_testpci_iter (grub_pci_device_t dev  __attribute__ ((unused)),
                   grub_pci_id_t pciid,
                   void *data) {

  struct grub_testpci_devlist* devlist = (struct grub_testpci_devlist*)data;

  char* device = grub_xasprintf ("%04x:%04x", pciid & 0xFFFF, pciid >> 16);
  for (int i = 0; i < devlist->n_devices; i++) {
    if (grub_strcasecmp(device, devlist->devices[i]) == 0) {
      grub_free(device);
      devlist->found = GRUB_ERR_NONE;
      return 1;
    }
  }

  grub_free(device);
  return 0;
}

static void
testpci_add_device_to_list(struct grub_testpci_devlist* devlist,
                           char* device)
{
  if (devlist->n_devices == devlist->s_devices) {
    devlist->s_devices *= 2;
    devlist->devices = grub_realloc(devlist->devices,
                                    devlist->s_devices * sizeof(char*));
    if (!(devlist->devices)) {
      return;
    }
  }
  devlist->devices[devlist->n_devices++] = grub_strdup(device);
}

static grub_err_t
grub_cmd_testpci (grub_extcmd_context_t ctxt,
                  int argc, char **args)
{

  struct grub_testpci_devlist devlist;

  devlist.found = GRUB_ERR_TEST_FAILURE;
  devlist.n_devices = 0;
  devlist.s_devices = argc + (ctxt->state[0].set ? 5 : 0);
  devlist.devices = grub_malloc(devlist.s_devices * sizeof(char*));
  if (!(devlist.devices)) {
    return GRUB_ERR_OUT_OF_MEMORY;
  }

  for (int i = 0; i < argc; i++) {
    testpci_add_device_to_list(&devlist, args[i]);
    if (!(devlist.devices)) {
      return GRUB_ERR_OUT_OF_MEMORY;
    }
  }

  /* device list from file */
  if (ctxt->state[0].set) {

    grub_file_t listfile = grub_file_open(ctxt->state[0].arg, GRUB_FILE_TYPE_NONE);
    if (!listfile)
      goto END_FILE;

    char *buf = NULL;
    while (grub_free (buf), (buf = grub_file_getline (listfile))) {

      /* remove comments */
      char *p = grub_strchr(buf, '#');
      if (p) {
        *p = '\0';
      }

      /* remove suffix spaces */
      p = buf + grub_strlen(buf) - 1;
      while (p >= buf && *p && grub_isspace(*p)) {
        *p-- = '\0';
      }

      /* remove prefix spaces */
      p = buf;
      while (*p && grub_isspace(*p)) {
        p++;
      }

      /* ignore empty */
      if (*p == '\0')
        continue;

      testpci_add_device_to_list(&devlist, p);
      if (!(devlist.devices)) {
        return GRUB_ERR_OUT_OF_MEMORY;
      }

    }
    grub_file_close (listfile);
  }
 END_FILE:

  for (int d = 0 ; d < devlist.n_devices; d++) {
    if (grub_strlen(devlist.devices[d]) != 9 || devlist.devices[d][4] != ':') {
      grub_printf("bad input device (%d) \"%s\", expected xxxx:xxxx\n", d, devlist.devices[d]);
    }
  }

  grub_pci_iterate (grub_testpci_iter, (void*)&devlist);

  for (int i = 0; i < devlist.n_devices; i++) {
    grub_free(devlist.devices[i]);
  }
  grub_free(devlist.devices);
  return devlist.found;
}

static grub_extcmd_t cmd;

GRUB_MOD_INIT(testpci)
{
  cmd = grub_register_extcmd ("testpci", grub_cmd_testpci, 0,
                              "[<devid> [...]] [--file <filename>]",
                              N_("Check if any of the PCI devices exist."), options);
}

GRUB_MOD_FINI(testpci)
{
  grub_unregister_extcmd (cmd);
}
