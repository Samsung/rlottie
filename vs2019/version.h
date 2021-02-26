#pragma once

#define STRINGIFY_HELPER(x)         #x
#define STRINGIFY_HELPER2(...)      STRINGIFY_HELPER(__VA_ARGS__)
#define TOSTRING(x)                 _T( STRINGIFY_HELPER(x) )
#define MAKE_STRING(x)              STRINGIFY_HELPER(x)

#define THISYEAR                    2020
#define CURYEAR                     STRINGIFY_HELPER2(THISYEAR)

// Change major or minor version when applicable, maintenance version will update automatically.
#define MAJOR_VERSION               100
#define MINOR_VERSION               0
#define MAINT_VERSION               0
#define BUILD_NUMBER                0

#define FILEVER                     MAJOR_VERSION,MINOR_VERSION,MAINT_VERSION,BUILD_NUMBER
#define PRODUCTVER                  MAJOR_VERSION,MINOR_VERSION,MAINT_VERSION,BUILD_NUMBER
#define FILE_VERSION_STRING         MAKE_STRING( MAJOR_VERSION.MINOR_VERSION.MAINT_VERSION.BUILD_NUMBER )
#define PRODUCT_VERSION_STRING      MAKE_STRING( MAJOR_VERSION.MINOR_VERSION.MAINT_VERSION.BUILD_NUMBER )

//Though the comments below is written by Snagit, we are keeping this comments for record
/* A note pertaining to why we are using several date ranges: (from a July 15, 2010 email)

Question: Stacy asked if we could simply use the Snagit copyright date range for the
separate binary components of Snagit.

Lawyer: "If the components are not based upon or derived from previous versions of
SNAGIT computer software program, such components would be deemed separate
original works of authorship (not "derivative works"), and technically should
use the dates of first publication for that component alone in the copyright
notice associated with that component and/or any derivative of that component."

Dean:    I think it is clear that we should use the unique copyright date range for components
of Snagit that are distribute with the product, but are not part of or derived from
the main Snagit program.  For example, a DLL that implements some feature
(Office add-in, text capture, etc) should have the copyright date range for when that
particular component has been distributed.  This is what we have always been doing
with our products in general.  If we have used Snagits date range (1996-2010) for
some separate components, we should probably fix those to have the correct date range.
*/

#define TRANSLATION_CODE_ENU_STRING    "040904b0\0"
#define TRANSLATION_ENU_PREFIX         0x0409
#define TRANSLATION_ENU_POSTFIX        1200

#define TRANSLATION_CODE_DEU_STRING    "040704E4\0"
#define TRANSLATION_DEU_PREFIX         0x0407
#define TRANSLATION_DEU_POSTFIX        1252

#define TRANSLATION_CODE_JPN_STRING    "041103A4\0"
#define TRANSLATION_JPN_PREFIX         0x0411
#define TRANSLATION_JPN_POSTFIX        932

#define TRANSLATION_CODE_FRA_STRING    "040904b0\0"
#define TRANSLATION_FRA_PREFIX         0x040c
#define TRANSLATION_FRA_POSTFIX        1252

#define COPYRIGHT_START_TO_CURRENT(START_YEAR)    "Copyright © " STRINGIFY_HELPER( START_YEAR ) "-" CURYEAR
#define COPYRIGHT_CURRENT                         "Copyright © " CURYEAR

#define PRODUCT_NAME                   "Camtasia"

#ifdef AFX_TARG_DEU
#define COPYRIGHT_NOTICE( START_YEAR )    COPYRIGHT_START_TO_CURRENT(START_YEAR) " TechSmith Corporation. Alle Rechte vorbehalten.\0"
#define COPYRIGHT_NOTICE_CURYEAR          COPYRIGHT_CURRENT " TechSmith Corporation. Alle Rechte vorbehalten.\0"
#define LEGAL_TRADEMARKS                  "Camtasia (r) ist eine eingetragene Marke der TechSmith Corporation.\0"
#define TRANSLATION_PREFIX                TRANSLATION_DEU_PREFIX
#define TRANSLATION_POSTFIX               TRANSLATION_DEU_POSTFIX
#define TRANSLATION_CODE_STRING           TRANSLATION_CODE_DEU_STRING
#elif AFX_TARG_JPN
#define COPYRIGHT_NOTICE( START_YEAR )    COPYRIGHT_START_TO_CURRENT(START_YEAR) " TechSmith Corporation. All rights reserved.\0"
#define COPYRIGHT_NOTICE_CURYEAR          COPYRIGHT_CURRENT " TechSmith Corporation. All rights reserved.\0"
#define LEGAL_TRADEMARKS                  "Camtasia (r) は TechSmith Corporation の登録商標です.\0"
#define TRANSLATION_PREFIX                TRANSLATION_JPN_PREFIX
#define TRANSLATION_POSTFIX               TRANSLATION_JPN_POSTFIX
#define TRANSLATION_CODE_STRING           TRANSLATION_CODE_JPN_STRING
#elif AFX_TARG_FRA
#define COPYRIGHT_NOTICE( START_YEAR )    COPYRIGHT_START_TO_CURRENT(START_YEAR) " TechSmith Corporation. Tous droits réservés.\0"
#define COPYRIGHT_NOTICE_CURYEAR          COPYRIGHT_CURRENT " TechSmith Corporation. Tous droits réservés.\0"
#define LEGAL_TRADEMARKS                  "Camtasia (r) est une marque déposée de TechSmith Corporation.\0"
#define TRANSLATION_PREFIX                TRANSLATION_FRA_PREFIX
#define TRANSLATION_POSTFIX               TRANSLATION_FRA_POSTFIX
#define TRANSLATION_CODE_STRING           TRANSLATION_CODE_FRA_STRING
// For future reference for ES the translation for 
// "Camtasia (r) is a registered trademark of TechSmith Corporation" will be
// "Camtasia (r) es una marca comercial registrada de TechSmith Corporation.\0"
#else
#define COPYRIGHT_NOTICE( START_YEAR )    COPYRIGHT_START_TO_CURRENT(START_YEAR) " TechSmith Corporation. All rights reserved.\0"
#define COPYRIGHT_NOTICE_CURYEAR          COPYRIGHT_CURRENT " TechSmith Corporation. All rights reserved.\0"
#define LEGAL_TRADEMARKS                  "Camtasia (r) is a registered trademark of TechSmith Corporation.\0"
#define TRANSLATION_PREFIX                TRANSLATION_ENU_PREFIX
#define TRANSLATION_POSTFIX               TRANSLATION_ENU_POSTFIX
#define TRANSLATION_CODE_STRING           TRANSLATION_CODE_ENU_STRING
#endif