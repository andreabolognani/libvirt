#include <config.h>

#include <unistd.h>

#include <sys/types.h>
#include <fcntl.h>

#include "testutils.h"

#ifdef WITH_QEMU

# include "internal.h"
# include "viralloc.h"
# include "qemu/qemu_alias.h"
# include "qemu/qemu_capabilities.h"
# include "qemu/qemu_command.h"
# include "qemu/qemu_domain.h"
# include "qemu/qemu_migration.h"
# include "qemu/qemu_process.h"
# include "datatypes.h"
# include "conf/storage_conf.h"
# include "cpu/cpu_map.h"
# include "virstring.h"
# include "storage/storage_driver.h"
# include "virmock.h"
# include "virfilewrapper.h"
# include "configmake.h"

# define LIBVIRT_QEMU_CAPSPRIV_H_ALLOW
# include "qemu/qemu_capspriv.h"

# include "testutilsqemu.h"

# define VIR_FROM_THIS VIR_FROM_QEMU

static virQEMUDriver driver;

static unsigned char *
fakeSecretGetValue(virSecretPtr obj ATTRIBUTE_UNUSED,
                   size_t *value_size,
                   unsigned int fakeflags ATTRIBUTE_UNUSED,
                   unsigned int internalFlags ATTRIBUTE_UNUSED)
{
    char *secret;
    if (VIR_STRDUP(secret, "AQCVn5hO6HzFAhAAq0NCv8jtJcIcE+HOBlMQ1A") < 0)
        return NULL;
    *value_size = strlen(secret);
    return (unsigned char *) secret;
}

static virSecretPtr
fakeSecretLookupByUsage(virConnectPtr conn,
                        int usageType,
                        const char *usageID)
{
    unsigned char uuid[VIR_UUID_BUFLEN];
    if (usageType == VIR_SECRET_USAGE_TYPE_VOLUME) {
        if (!STRPREFIX(usageID, "/storage/guest_disks/")) {
            virReportError(VIR_ERR_INTERNAL_ERROR,
                           "test provided invalid volume storage prefix '%s'",
                           usageID);
            return NULL;
        }
    } else if (STRNEQ(usageID, "mycluster_myname")) {
        virReportError(VIR_ERR_INTERNAL_ERROR,
                       "test provided incorrect usage '%s'", usageID);
        return NULL;
    }

    if (virUUIDGenerate(uuid) < 0)
        return NULL;

    return virGetSecret(conn, uuid, usageType, usageID);
}

static virSecretPtr
fakeSecretLookupByUUID(virConnectPtr conn,
                       const unsigned char *uuid)
{
    /* NB: This mocked value could be "tls" or "volume" depending on
     * which test is being run, we'll leave at NONE (or 0) */
    return virGetSecret(conn, uuid, VIR_SECRET_USAGE_TYPE_NONE, "");
}

static virSecretDriver fakeSecretDriver = {
    .connectNumOfSecrets = NULL,
    .connectListSecrets = NULL,
    .secretLookupByUUID = fakeSecretLookupByUUID,
    .secretLookupByUsage = fakeSecretLookupByUsage,
    .secretDefineXML = NULL,
    .secretGetXMLDesc = NULL,
    .secretSetValue = NULL,
    .secretGetValue = fakeSecretGetValue,
    .secretUndefine = NULL,
};


# define STORAGE_POOL_XML_PATH "storagepoolxml2xmlout/"
static const unsigned char fakeUUID[VIR_UUID_BUFLEN] = "fakeuuid";

static virStoragePoolPtr
fakeStoragePoolLookupByName(virConnectPtr conn,
                            const char *name)
{
    char *xmlpath = NULL;
    virStoragePoolPtr ret = NULL;

    if (STRNEQ(name, "inactive")) {
        if (virAsprintf(&xmlpath, "%s/%s%s.xml",
                        abs_srcdir,
                        STORAGE_POOL_XML_PATH,
                        name) < 0)
            return NULL;

        if (!virFileExists(xmlpath)) {
            virReportError(VIR_ERR_NO_STORAGE_POOL,
                           "File '%s' not found", xmlpath);
            goto cleanup;
        }
    }

    ret = virGetStoragePool(conn, name, fakeUUID, NULL, NULL);

 cleanup:
    VIR_FREE(xmlpath);
    return ret;
}


static virStorageVolPtr
fakeStorageVolLookupByName(virStoragePoolPtr pool,
                           const char *name)
{
    char **volinfo = NULL;
    virStorageVolPtr ret = NULL;

    if (STREQ(pool->name, "inactive")) {
        virReportError(VIR_ERR_OPERATION_INVALID,
                       "storage pool '%s' is not active", pool->name);
        return NULL;
    }

    if (STREQ(name, "nonexistent")) {
        virReportError(VIR_ERR_NO_STORAGE_VOL,
                       "no storage vol with matching name '%s'", name);
        return NULL;
    }

    if (!strchr(name, '+'))
        goto fallback;

    if (!(volinfo = virStringSplit(name, "+", 2)))
        return NULL;

    if (!volinfo[1])
        goto fallback;

    ret = virGetStorageVol(pool->conn, pool->name, volinfo[1], volinfo[0],
                           NULL, NULL);

 cleanup:
    virStringListFree(volinfo);
    return ret;

 fallback:
    ret = virGetStorageVol(pool->conn, pool->name, name, "block", NULL, NULL);
    goto cleanup;
}

static int
fakeStorageVolGetInfo(virStorageVolPtr vol,
                      virStorageVolInfoPtr info)
{
    memset(info, 0, sizeof(*info));

    info->type = virStorageVolTypeFromString(vol->key);

    if (info->type < 0) {
        virReportError(VIR_ERR_INTERNAL_ERROR,
                       "Invalid volume type '%s'", vol->key);
        return -1;
    }

    return 0;
}


static char *
fakeStorageVolGetPath(virStorageVolPtr vol)
{
    char *ret = NULL;

    ignore_value(virAsprintf(&ret, "/some/%s/device/%s", vol->key, vol->name));

    return ret;
}


static char *
fakeStoragePoolGetXMLDesc(virStoragePoolPtr pool,
                          unsigned int flags_unused ATTRIBUTE_UNUSED)
{
    char *xmlpath = NULL;
    char *xmlbuf = NULL;

    if (STREQ(pool->name, "inactive")) {
        virReportError(VIR_ERR_NO_STORAGE_POOL, NULL);
        return NULL;
    }

    if (virAsprintf(&xmlpath, "%s/%s%s.xml",
                    abs_srcdir,
                    STORAGE_POOL_XML_PATH,
                    pool->name) < 0)
        return NULL;

    if (virTestLoadFile(xmlpath, &xmlbuf) < 0) {
        virReportError(VIR_ERR_INTERNAL_ERROR,
                       "failed to load XML file '%s'",
                       xmlpath);
        goto cleanup;
    }

 cleanup:
    VIR_FREE(xmlpath);

    return xmlbuf;
}

static int
fakeStoragePoolIsActive(virStoragePoolPtr pool)
{
    if (STREQ(pool->name, "inactive"))
        return 0;

    return 1;
}

/* Test storage pool implementation
 *
 * These functions aid testing of storage pool related stuff when creating a
 * qemu command line.
 *
 * There are a few "magic" values to pass to these functions:
 *
 * 1) "inactive" as a pool name to create an inactive pool. All other names are
 * interpreted as file names in storagepoolxml2xmlout/ and are used as the
 * definition for the pool. If the file doesn't exist the pool doesn't exist.
 *
 * 2) "nonexistent" returns an error while looking up a volume. Otherwise
 * pattern VOLUME_TYPE+VOLUME_PATH can be used to simulate a volume in a pool.
 * This creates a fake path for this volume. If the '+' sign is omitted, block
 * type is assumed.
 */
static virStorageDriver fakeStorageDriver = {
    .storagePoolLookupByName = fakeStoragePoolLookupByName,
    .storageVolLookupByName = fakeStorageVolLookupByName,
    .storagePoolGetXMLDesc = fakeStoragePoolGetXMLDesc,
    .storageVolGetPath = fakeStorageVolGetPath,
    .storageVolGetInfo = fakeStorageVolGetInfo,
    .storagePoolIsActive = fakeStoragePoolIsActive,
};


/* virNetDevOpenvswitchGetVhostuserIfname mocks a portdev name - handle that */
static virNWFilterBindingPtr
fakeNWFilterBindingLookupByPortDev(virConnectPtr conn,
                                   const char *portdev)
{
    if (STREQ(portdev, "vhost-user0"))
        return virGetNWFilterBinding(conn, "fake_vnet0", "fakeFilterName");

    virReportError(VIR_ERR_NO_NWFILTER_BINDING,
                   "no nwfilter binding for port dev '%s'", portdev);
    return NULL;
}


static int
fakeNWFilterBindingDelete(virNWFilterBindingPtr binding ATTRIBUTE_UNUSED)
{
    return 0;
}


static virNWFilterDriver fakeNWFilterDriver = {
    .nwfilterBindingLookupByPortDev = fakeNWFilterBindingLookupByPortDev,
    .nwfilterBindingDelete = fakeNWFilterBindingDelete,
};

typedef enum {
    FLAG_EXPECT_FAILURE     = 1 << 0,
    FLAG_EXPECT_PARSE_ERROR = 1 << 1,
    FLAG_FIPS               = 1 << 2,
    FLAG_REAL_CAPS          = 1 << 3,
    FLAG_SKIP_LEGACY_CPUS   = 1 << 4,
} virQemuJSON2ArgvTestFlags;

struct testInfo {
    const char *name;
    const char *suffix;
    virQEMUCapsPtr qemuCaps;
    const char *migrateFrom;
    int migrateFd;
    unsigned int flags;
    unsigned int parseFlags;
};


static int
testAddCPUModels(virQEMUCapsPtr caps, bool skipLegacy)
{
    virArch arch = virQEMUCapsGetArch(caps);
    const char *x86Models[] = {
        "Opteron_G3", "Opteron_G2", "Opteron_G1",
        "Nehalem", "Penryn", "Conroe",
        "Haswell-noTSX", "Haswell",
    };
    const char *x86LegacyModels[] = {
        "n270", "athlon", "pentium3", "pentium2", "pentium",
        "486", "coreduo", "kvm32", "qemu32", "kvm64",
        "core2duo", "phenom", "qemu64",
    };
    const char *armModels[] = {
        "cortex-a9", "cortex-a8", "cortex-a57", "cortex-a53",
    };
    const char *ppc64Models[] = {
        "POWER8", "POWER7",
    };
    const char *s390xModels[] = {
        "z990", "zEC12", "z13",
    };

    if (ARCH_IS_X86(arch)) {
        if (virQEMUCapsAddCPUDefinitions(caps, VIR_DOMAIN_VIRT_KVM, x86Models,
                                         ARRAY_CARDINALITY(x86Models),
                                         VIR_DOMCAPS_CPU_USABLE_UNKNOWN) < 0 ||
            virQEMUCapsAddCPUDefinitions(caps, VIR_DOMAIN_VIRT_QEMU, x86Models,
                                         ARRAY_CARDINALITY(x86Models),
                                         VIR_DOMCAPS_CPU_USABLE_UNKNOWN) < 0)
            return -1;

        if (!skipLegacy) {
            if (virQEMUCapsAddCPUDefinitions(caps, VIR_DOMAIN_VIRT_KVM,
                                             x86LegacyModels,
                                             ARRAY_CARDINALITY(x86LegacyModels),
                                             VIR_DOMCAPS_CPU_USABLE_UNKNOWN) < 0 ||
                virQEMUCapsAddCPUDefinitions(caps, VIR_DOMAIN_VIRT_QEMU,
                                             x86LegacyModels,
                                             ARRAY_CARDINALITY(x86LegacyModels),
                                             VIR_DOMCAPS_CPU_USABLE_UNKNOWN) < 0)
                return -1;
        }
    } else if (ARCH_IS_ARM(arch)) {
        if (virQEMUCapsAddCPUDefinitions(caps, VIR_DOMAIN_VIRT_KVM, armModels,
                                         ARRAY_CARDINALITY(armModels),
                                         VIR_DOMCAPS_CPU_USABLE_UNKNOWN) < 0 ||
            virQEMUCapsAddCPUDefinitions(caps, VIR_DOMAIN_VIRT_QEMU, armModels,
                                         ARRAY_CARDINALITY(armModels),
                                         VIR_DOMCAPS_CPU_USABLE_UNKNOWN) < 0)
            return -1;
    } else if (ARCH_IS_PPC64(arch)) {
        if (virQEMUCapsAddCPUDefinitions(caps, VIR_DOMAIN_VIRT_KVM, ppc64Models,
                                         ARRAY_CARDINALITY(ppc64Models),
                                         VIR_DOMCAPS_CPU_USABLE_UNKNOWN) < 0 ||
            virQEMUCapsAddCPUDefinitions(caps, VIR_DOMAIN_VIRT_QEMU, ppc64Models,
                                         ARRAY_CARDINALITY(ppc64Models),
                                         VIR_DOMCAPS_CPU_USABLE_UNKNOWN) < 0)
            return -1;
    } else if (ARCH_IS_S390(arch)) {
        if (virQEMUCapsAddCPUDefinitions(caps, VIR_DOMAIN_VIRT_KVM, s390xModels,
                                         ARRAY_CARDINALITY(s390xModels),
                                         VIR_DOMCAPS_CPU_USABLE_UNKNOWN) < 0)
            return -1;
    }

    return 0;
}


static int
testUpdateQEMUCaps(const struct testInfo *info,
                   virDomainObjPtr vm,
                   virCapsPtr caps)
{
    int ret = -1;

    if (!caps)
        goto cleanup;

    virQEMUCapsSetArch(info->qemuCaps, vm->def->os.arch);

    virQEMUCapsInitQMPBasicArch(info->qemuCaps);

    if (testAddCPUModels(info->qemuCaps,
                         !!(info->flags & FLAG_SKIP_LEGACY_CPUS)) < 0)
        goto cleanup;

    virQEMUCapsInitHostCPUModel(info->qemuCaps, caps->host.arch,
                                VIR_DOMAIN_VIRT_KVM);
    virQEMUCapsInitHostCPUModel(info->qemuCaps, caps->host.arch,
                                VIR_DOMAIN_VIRT_QEMU);

    ret = 0;

 cleanup:
    return ret;
}


static int
testCheckExclusiveFlags(int flags)
{
    virCheckFlags(FLAG_EXPECT_FAILURE |
                  FLAG_EXPECT_PARSE_ERROR |
                  FLAG_FIPS |
                  FLAG_REAL_CAPS |
                  FLAG_SKIP_LEGACY_CPUS |
                  0, -1);

    VIR_EXCLUSIVE_FLAGS_RET(FLAG_REAL_CAPS, FLAG_SKIP_LEGACY_CPUS, -1);
    return 0;
}


# define JSON_BUFSIZE (10*1024*1024)


static int
testCompareJSONToArgv(const void *data)
{
    struct testInfo *info = (void *) data;
    char *json = NULL;
    char *args = NULL;
    char *migrateURI = NULL;
    char *actualargv = NULL;
    const char *suffix = info->suffix;
    unsigned int flags = info->flags;
    unsigned int parseFlags = info->parseFlags;
    int ret = -1;
    virDomainObjPtr vm = NULL;
    virDomainChrSourceDef monitor_chr;
    virConnectPtr conn;
    char *log = NULL;
    virCommandPtr cmd = NULL;
    size_t i;
    qemuDomainObjPrivatePtr priv = NULL;
    VIR_AUTOFREE(char *) buf = NULL;

    memset(&monitor_chr, 0, sizeof(monitor_chr));

    if (!(conn = virGetConnect()))
        goto cleanup;

    if (!suffix)
        suffix = "";

    conn->secretDriver = &fakeSecretDriver;
    conn->storageDriver = &fakeStorageDriver;
    conn->nwfilterDriver = &fakeNWFilterDriver;

    virSetConnectInterface(conn);
    virSetConnectNetwork(conn);
    virSetConnectNWFilter(conn);
    virSetConnectNodeDev(conn);
    virSetConnectSecret(conn);
    virSetConnectStorage(conn);

    if (virQEMUCapsGet(info->qemuCaps, QEMU_CAPS_ENABLE_FIPS))
        flags |= FLAG_FIPS;

    if (testCheckExclusiveFlags(info->flags) < 0)
        goto cleanup;

    if (qemuTestCapsCacheInsert(driver.qemuCapsCache, info->qemuCaps) < 0)
        goto cleanup;

    if (virAsprintf(&json, "%s/qemujson2argvdata/%s.json",
                    abs_srcdir, info->name) < 0 ||
        virAsprintf(&args, "%s/qemujson2argvdata/%s%s.args",
                    abs_srcdir, info->name, suffix) < 0)
        goto cleanup;

    if (info->migrateFrom &&
        !(migrateURI = qemuMigrationDstGetURI(info->migrateFrom,
                                              info->migrateFd)))
        goto cleanup;

    if (!(vm = virDomainObjNew(driver.xmlopt)))
        goto cleanup;

    if (virFileReadAll(json, JSON_BUFSIZE, &buf) < 0)
        goto cleanup;

    parseFlags |= VIR_DOMAIN_DEF_PARSE_INACTIVE;
    if (!(vm->def = virDomainDefParseJSONString(buf, driver.caps, driver.xmlopt,
                                                NULL, parseFlags))) {
        if (flags & FLAG_EXPECT_PARSE_ERROR)
            goto ok;
        goto cleanup;
    }
    if (flags & FLAG_EXPECT_PARSE_ERROR) {
        VIR_TEST_DEBUG("passed instead of expected parse error");
        goto cleanup;
    }
    priv = vm->privateData;

    if (virBitmapParse("0-3", &priv->autoNodeset, 4) < 0)
        goto cleanup;

    if (!virDomainDefCheckABIStability(vm->def, vm->def, driver.xmlopt)) {
        VIR_TEST_DEBUG("ABI stability check failed on %s", json);
        goto cleanup;
    }

    vm->def->id = -1;

    if (qemuProcessPrepareMonitorChr(&monitor_chr, priv->libDir) < 0)
        goto cleanup;

    if (!(info->flags & FLAG_REAL_CAPS) &&
        testUpdateQEMUCaps(info, vm, driver.caps) < 0)
        goto cleanup;

    log = virTestLogContentAndReset();
    VIR_FREE(log);
    virResetLastError();

    for (i = 0; i < vm->def->nhostdevs; i++) {
        virDomainHostdevDefPtr hostdev = vm->def->hostdevs[i];

        if (hostdev->mode == VIR_DOMAIN_HOSTDEV_MODE_SUBSYS &&
            hostdev->source.subsys.type == VIR_DOMAIN_HOSTDEV_SUBSYS_TYPE_PCI &&
            hostdev->source.subsys.u.pci.backend == VIR_DOMAIN_HOSTDEV_PCI_BACKEND_DEFAULT) {
            hostdev->source.subsys.u.pci.backend = VIR_DOMAIN_HOSTDEV_PCI_BACKEND_KVM;
        }
    }

    if (vm->def->vsock) {
        virDomainVsockDefPtr vsock = vm->def->vsock;
        qemuDomainVsockPrivatePtr vsockPriv =
            (qemuDomainVsockPrivatePtr)vsock->privateData;

        if (vsock->auto_cid == VIR_TRISTATE_BOOL_YES)
            vsock->guest_cid = 42;

        vsockPriv->vhostfd = 6789;
    }

    if (vm->def->tpm) {
        switch (vm->def->tpm->type) {
        case VIR_DOMAIN_TPM_TYPE_EMULATOR:
            VIR_FREE(vm->def->tpm->data.emulator.source.data.file.path);
            if (VIR_STRDUP(vm->def->tpm->data.emulator.source.data.file.path,
                           "/dev/test") < 0)
                goto cleanup;
            vm->def->tpm->data.emulator.source.type = VIR_DOMAIN_CHR_TYPE_FILE;
            break;
        case VIR_DOMAIN_TPM_TYPE_PASSTHROUGH:
        case VIR_DOMAIN_TPM_TYPE_LAST:
            break;
       }
    }

    if (!(cmd = qemuProcessCreatePretendCmd(&driver, vm, migrateURI,
                                            (flags & FLAG_FIPS), false,
                                            VIR_QEMU_PROCESS_START_COLD))) {
        if (flags & FLAG_EXPECT_FAILURE)
            goto ok;
        goto cleanup;
    }
    if (flags & FLAG_EXPECT_FAILURE) {
        VIR_TEST_DEBUG("passed instead of expected failure");
        goto cleanup;
    }

    if (!(actualargv = virCommandToString(cmd, false)))
        goto cleanup;

    if (virTestCompareToFile(actualargv, args) < 0)
        goto cleanup;

    ret = 0;

 ok:
    if (ret == 0 && flags & FLAG_EXPECT_FAILURE) {
        ret = -1;
        VIR_TEST_DEBUG("Error expected but there wasn't any.\n");
        goto cleanup;
    }
    if (!virTestOOMActive()) {
        if (flags & FLAG_EXPECT_FAILURE) {
            if ((log = virTestLogContentAndReset()))
                VIR_TEST_DEBUG("Got expected error: \n%s", log);
        }
        virResetLastError();
        ret = 0;
    }

 cleanup:
    VIR_FREE(log);
    VIR_FREE(actualargv);
    virDomainChrSourceDefClear(&monitor_chr);
    virCommandFree(cmd);
    virObjectUnref(vm);
    virSetConnectSecret(NULL);
    virSetConnectStorage(NULL);
    virObjectUnref(conn);
    VIR_FREE(migrateURI);
    VIR_FREE(json);
    VIR_FREE(args);
    return ret;
}

# define TEST_CAPS_PATH abs_srcdir "/qemucapabilitiesdata"

typedef enum {
    ARG_QEMU_CAPS,
    ARG_GIC,
    ARG_MIGRATE_FROM,
    ARG_MIGRATE_FD,
    ARG_FLAGS,
    ARG_PARSEFLAGS,
    ARG_CAPS_ARCH,
    ARG_CAPS_VER,
    ARG_END,
} testInfoArgName;

static int
testInfoSetArgs(struct testInfo *info,
                virHashTablePtr capslatest, ...)
{
    va_list argptr;
    testInfoArgName argname;
    virQEMUCapsPtr qemuCaps = NULL;
    int gic = GIC_NONE;
    char *capsarch = NULL;
    char *capsver = NULL;
    VIR_AUTOFREE(char *) capsfile = NULL;
    int flag;
    int ret = -1;

    va_start(argptr, capslatest);
    argname = va_arg(argptr, testInfoArgName);
    while (argname != ARG_END) {
        switch (argname) {
        case ARG_QEMU_CAPS:
            if (qemuCaps || !(qemuCaps = virQEMUCapsNew()))
                goto cleanup;

            while ((flag = va_arg(argptr, int)) < QEMU_CAPS_LAST)
                virQEMUCapsSet(qemuCaps, flag);

            /* Some tests are run with NONE capabilities, which is just
             * another name for QEMU_CAPS_LAST. If that is the case the
             * arguments look like this :
             *
             *   ARG_QEMU_CAPS, NONE, QEMU_CAPS_LAST, ARG_END
             *
             * Fetch one argument more and if it is QEMU_CAPS_LAST then
             * break from the switch() to force getting next argument
             * in the line. If it is not QEMU_CAPS_LAST then we've
             * fetched real ARG_* and we must process it.
             */
            if ((flag = va_arg(argptr, int)) != QEMU_CAPS_LAST) {
                argname = flag;
                continue;
            }

            break;

        case ARG_GIC:
            gic = va_arg(argptr, int);
            break;

        case ARG_MIGRATE_FROM:
            info->migrateFrom = va_arg(argptr, char *);
            break;

        case ARG_MIGRATE_FD:
            info->migrateFd = va_arg(argptr, int);
            break;

        case ARG_FLAGS:
            info->flags = va_arg(argptr, int);
            break;

        case ARG_PARSEFLAGS:
            info->parseFlags = va_arg(argptr, int);
            break;

        case ARG_CAPS_ARCH:
            capsarch = va_arg(argptr, char *);
            break;

        case ARG_CAPS_VER:
            capsver = va_arg(argptr, char *);
            break;

        case ARG_END:
        default:
            fprintf(stderr, "Unexpected test info argument");
            goto cleanup;
        }

        argname = va_arg(argptr, testInfoArgName);
    }

    if (!!capsarch ^ !!capsver) {
        fprintf(stderr, "ARG_CAPS_ARCH and ARG_CAPS_VER "
                        "must be specified together.\n");
        goto cleanup;
    }

    if (qemuCaps && (capsarch || capsver)) {
        fprintf(stderr, "ARG_QEMU_CAPS can not be combined with ARG_CAPS_ARCH "
                        "or ARG_CAPS_VER\n");
        goto cleanup;
    }

    if (!qemuCaps && capsarch && capsver) {
        bool stripmachinealiases = false;

        if (STREQ(capsver, "latest")) {
            if (VIR_STRDUP(capsfile, virHashLookup(capslatest, capsarch)) < 0)
                goto cleanup;
            stripmachinealiases = true;
        } else if (virAsprintf(&capsfile, "%s/caps_%s.%s.xml",
                               TEST_CAPS_PATH, capsver, capsarch) < 0) {
            goto cleanup;
        }

        if (!(qemuCaps = qemuTestParseCapabilitiesArch(virArchFromString(capsarch),
                                                       capsfile))) {
            goto cleanup;
        }

        if (stripmachinealiases)
            virQEMUCapsStripMachineAliases(qemuCaps);
        info->flags |= FLAG_REAL_CAPS;
    }

    if (!qemuCaps) {
        fprintf(stderr, "No qemuCaps generated\n");
        goto cleanup;
    }
    VIR_STEAL_PTR(info->qemuCaps, qemuCaps);

    if (gic != GIC_NONE && testQemuCapsSetGIC(info->qemuCaps, gic) < 0)
        goto cleanup;

    ret = 0;

 cleanup:
    virObjectUnref(qemuCaps);
    va_end(argptr);

    return ret;
}

static void
testInfoClear(struct testInfo *info)
{
    virObjectUnref(info->qemuCaps);
}

# define FAKEROOTDIRTEMPLATE abs_builddir "/fakerootdir-XXXXXX"

static int
mymain(void)
{
    int ret = 0, i;
    char *fakerootdir;
    const char *archs[] = {
        "aarch64",
        "ppc64",
        "riscv64",
        "s390x",
        "x86_64",
    };
    virHashTablePtr capslatest = NULL;

    if (VIR_STRDUP_QUIET(fakerootdir, FAKEROOTDIRTEMPLATE) < 0) {
        fprintf(stderr, "Out of memory\n");
        abort();
    }

    if (!mkdtemp(fakerootdir)) {
        fprintf(stderr, "Cannot create fakerootdir");
        abort();
    }

    setenv("LIBVIRT_FAKE_ROOT_DIR", fakerootdir, 1);

    /* Set the timezone because we are mocking the time() function.
     * If we don't do that, then localtime() may return unpredictable
     * results. In order to detect things that just work by a blind
     * chance, we need to set an virtual timezone that no libvirt
     * developer resides in. */
    if (setenv("TZ", "VIR00:30", 1) < 0) {
        perror("setenv");
        return EXIT_FAILURE;
    }

    if (qemuTestDriverInit(&driver) < 0)
        return EXIT_FAILURE;

    driver.privileged = true;

    VIR_FREE(driver.config->defaultTLSx509certdir);
    if (VIR_STRDUP_QUIET(driver.config->defaultTLSx509certdir, "/etc/pki/qemu") < 0)
        return EXIT_FAILURE;
    VIR_FREE(driver.config->vncTLSx509certdir);
    if (VIR_STRDUP_QUIET(driver.config->vncTLSx509certdir, "/etc/pki/libvirt-vnc") < 0)
        return EXIT_FAILURE;
    VIR_FREE(driver.config->spiceTLSx509certdir);
    if (VIR_STRDUP_QUIET(driver.config->spiceTLSx509certdir, "/etc/pki/libvirt-spice") < 0)
        return EXIT_FAILURE;
    VIR_FREE(driver.config->chardevTLSx509certdir);
    if (VIR_STRDUP_QUIET(driver.config->chardevTLSx509certdir, "/etc/pki/libvirt-chardev") < 0)
        return EXIT_FAILURE;
    VIR_FREE(driver.config->vxhsTLSx509certdir);
    if (VIR_STRDUP_QUIET(driver.config->vxhsTLSx509certdir, "/etc/pki/libvirt-vxhs/dummy,path") < 0)
        return EXIT_FAILURE;
    VIR_FREE(driver.config->nbdTLSx509certdir);
    if (VIR_STRDUP_QUIET(driver.config->nbdTLSx509certdir, "/etc/pki/libvirt-nbd/dummy,path") < 0)
        return EXIT_FAILURE;

    VIR_FREE(driver.config->hugetlbfs);
    if (VIR_ALLOC_N(driver.config->hugetlbfs, 2) < 0)
        return EXIT_FAILURE;
    driver.config->nhugetlbfs = 2;
    if (VIR_STRDUP(driver.config->hugetlbfs[0].mnt_dir, "/dev/hugepages2M") < 0 ||
        VIR_STRDUP(driver.config->hugetlbfs[1].mnt_dir, "/dev/hugepages1G") < 0)
        return EXIT_FAILURE;
    driver.config->hugetlbfs[0].size = 2048;
    driver.config->hugetlbfs[0].deflt = true;
    driver.config->hugetlbfs[1].size = 1048576;
    driver.config->spiceTLS = 1;
    if (VIR_STRDUP_QUIET(driver.config->spicePassword, "123456") < 0)
        return EXIT_FAILURE;
    VIR_FREE(driver.config->memoryBackingDir);
    if (VIR_STRDUP_QUIET(driver.config->memoryBackingDir, "/var/lib/libvirt/qemu/ram") < 0)
        return EXIT_FAILURE;
    VIR_FREE(driver.config->nvramDir);
    if (VIR_STRDUP(driver.config->nvramDir, "/var/lib/libvirt/qemu/nvram") < 0)
        return EXIT_FAILURE;

    capslatest = virHashCreate(4, virHashValueFree);
    if (!capslatest)
        return EXIT_FAILURE;

    VIR_TEST_VERBOSE("\n");

    for (i = 0; i < ARRAY_CARDINALITY(archs); ++i) {
        char *cap = testQemuGetLatestCapsForArch(abs_srcdir "/qemucapabilitiesdata",
                                                 archs[i], "xml");

        if (!cap || virHashAddEntry(capslatest, archs[i], cap) < 0)
            return EXIT_FAILURE;

        VIR_TEST_VERBOSE("latest caps for %s: %s\n", archs[i], cap);
    }

    VIR_TEST_VERBOSE("\n");

    virFileWrapperAddPrefix(SYSCONFDIR "/qemu/firmware",
                            abs_srcdir "/qemufirmwaredata/etc/qemu/firmware");
    virFileWrapperAddPrefix(PREFIX "/share/qemu/firmware",
                            abs_srcdir "/qemufirmwaredata/usr/share/qemu/firmware");
    virFileWrapperAddPrefix("/home/user/.config/qemu/firmware",
                            abs_srcdir "/qemufirmwaredata/home/user/.config/qemu/firmware");

/**
 * The following set of macros allows testing of JSON -> argv conversion with a
 * real set of capabilities gathered from a real qemu copy. It is desired to use
 * these for positive test cases as it provides combinations of flags which
 * can be met in real life.
 *
 * The capabilities are taken from the real capabilities stored in
 * tests/qemucapabilitiesdata.
 *
 * It is suggested to use the DO_TEST_CAPS_LATEST macro which always takes the
 * most recent capability set. In cases when the new code would change behaviour
 * the test cases should be forked using DO_TEST_CAPS_VER with the appropriate
 * version.
 */
# define DO_TEST_INTERNAL(_name, _suffix, ...) \
    do { \
        static struct testInfo info = { \
            .name = _name, \
            .suffix = _suffix, \
        }; \
        if (testInfoSetArgs(&info, capslatest, \
                            __VA_ARGS__, ARG_END) < 0) \
            return EXIT_FAILURE; \
        if (virTestRun("QEMU JSON-2-ARGV " _name _suffix, \
                       testCompareJSONToArgv, &info) < 0) \
            ret = -1; \
        testInfoClear(&info); \
    } while (0)

# define DO_TEST_CAPS_INTERNAL(name, arch, ver, ...) \
    DO_TEST_INTERNAL(name, "." arch "-" ver, \
                     ARG_CAPS_ARCH, arch, \
                     ARG_CAPS_VER, ver, \
                     __VA_ARGS__)

# define DO_TEST_CAPS_ARCH_VER(name, arch, ver) \
    DO_TEST_CAPS_INTERNAL(name, arch, ver, ARG_END)

# define DO_TEST_CAPS_VER(name, ver) \
    DO_TEST_CAPS_ARCH_VER(name, "x86_64", ver)

# define DO_TEST_CAPS_ARCH_LATEST_FULL(name, arch, ...) \
    DO_TEST_CAPS_INTERNAL(name, arch, "latest", __VA_ARGS__)

# define DO_TEST_CAPS_ARCH_LATEST(name, arch) \
    DO_TEST_CAPS_ARCH_LATEST_FULL(name, arch, ARG_END)

# define DO_TEST_CAPS_LATEST(name) \
    DO_TEST_CAPS_ARCH_LATEST(name, "x86_64")

# define DO_TEST_CAPS_LATEST_FAILURE(name) \
    DO_TEST_CAPS_ARCH_LATEST_FULL(name, "x86_64", \
                                  ARG_FLAGS, FLAG_EXPECT_FAILURE)

# define DO_TEST_CAPS_LATEST_PARSE_ERROR(name) \
    DO_TEST_CAPS_ARCH_LATEST_FULL(name, "x86_64", \
                                  ARG_FLAGS, FLAG_EXPECT_PARSE_ERROR)


# define DO_TEST_FULL(name, ...) \
    DO_TEST_INTERNAL(name, "", \
                     __VA_ARGS__, QEMU_CAPS_LAST)

/* All the following macros require an explicit QEMU_CAPS_* list
 * at the end of the argument list, or the NONE placeholder.
 * */
# define DO_TEST(name, ...) \
    DO_TEST_FULL(name, \
                 ARG_QEMU_CAPS, __VA_ARGS__)

# define DO_TEST_GIC(name, gic, ...) \
    DO_TEST_FULL(name, \
                 ARG_GIC, gic, \
                 ARG_QEMU_CAPS, __VA_ARGS__)

# define DO_TEST_FAILURE(name, ...) \
    DO_TEST_FULL(name, \
                 ARG_FLAGS, FLAG_EXPECT_FAILURE, \
                 ARG_QEMU_CAPS, __VA_ARGS__)

# define DO_TEST_PARSE_ERROR(name, ...) \
    DO_TEST_FULL(name, \
                 ARG_FLAGS, FLAG_EXPECT_PARSE_ERROR | FLAG_EXPECT_FAILURE, \
                 ARG_QEMU_CAPS, __VA_ARGS__)

# define NONE QEMU_CAPS_LAST

    /* Unset or set all envvars here that are copied in qemudBuildCommandLine
     * using ADD_ENV_COPY, otherwise these tests may fail due to unexpected
     * values for these envvars */
    setenv("PATH", "/bin", 1);
    setenv("USER", "test", 1);
    setenv("LOGNAME", "test", 1);
    setenv("HOME", "/home/test", 1);
    unsetenv("TMPDIR");
    unsetenv("LD_PRELOAD");
    unsetenv("LD_LIBRARY_PATH");
    unsetenv("QEMU_AUDIO_DRV");
    unsetenv("SDL_AUDIODRIVER");

    DO_TEST_CAPS_LATEST("tiny");

    if (getenv("LIBVIRT_SKIP_CLEANUP") == NULL)
        virFileDeleteTree(fakerootdir);

    VIR_FREE(driver.config->nbdTLSx509certdir);
    qemuTestDriverFree(&driver);
    VIR_FREE(fakerootdir);
    virHashFree(capslatest);
    virFileWrapperClearPrefixes();

    return ret == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}

VIR_TEST_MAIN_PRELOAD(mymain,
                      abs_builddir "/.libs/qemuxml2argvmock.so",
                      abs_builddir "/.libs/virrandommock.so",
                      abs_builddir "/.libs/qemucpumock.so",
                      abs_builddir "/.libs/virpcimock.so")

#else

int main(void)
{
    return EXIT_AM_SKIP;
}

#endif /* WITH_QEMU */
