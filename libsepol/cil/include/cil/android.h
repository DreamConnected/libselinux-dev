#ifndef _SEPOL_ANDROID_H_
#define _SEPOL_ANDROID_H_
#include <cil/cil.h>

#define PLAT_VERS "curr"
#define PLAT_ID "p"
#define NON_PLAT_ID "n"

/*
 * cil_attrib_mapping - extract attributizable elements of the policy in srcdb
 * and create the mapping file necessary to link the platform and non-platform
 * policy files after non-platform policy attributization.
 *   mdb - uninitialized cil_db reference to the resulting policy. Caller
 *         responsibility to destroy.
 *   srcdb - initialized and parsed cil_db reference to source public policy.
 */
int cil_android_attrib_mapping(struct cil_db **mdb, struct cil_db *srcdb, const char *num);

/*
 * cil_attributize - extract attributizable elements of the policy in srcdb and
 * convert all usage of those elements in tgtdb to versioned attributes.
 */
int cil_android_attributize(struct cil_db *tgtdb, struct cil_db *srcdb, const char *num);

#endif /* _SEPOL_ANDROID_H_ */
