// SPDX-License-Identifier: GPL-2.0-only
#include "os44_eua_policy.h"
struct os44_eua_policy_record os44_eua_policy[12]={
 {"SKhynix","HN8T171EJKX151","",264,&os44_hynix_eua_ops},
 {"SKhynix","HN8T271EJKX152","",528,&os44_hynix_eua_ops},
 {"SKhynix","HN8T371ZJKX154","",1056,&os44_hynix_eua_ops},
 {"SKhynix","HN8T281EJKX158","",520,&os44_hynix_eua_ops},
 {"SAMSUNG","KLUEG4RHKF-F0H1","",264,&os44_samsung_eua_ops},
 {"SAMSUNG","KLUFG8RHKF-F0H1","",528,&os44_samsung_eua_ops},
 {"SAMSUNG","KLUGG8NHKB-F0H1","",1056,&os44_samsung_eua_ops},
 {"MICRON","MT256GBGBB2U41","",264,&os44_micron_eua_ops},
 {"MICRON","MT512GBGBB4U41","",528,&os44_micron_eua_ops},
 {"XBSTOR","XBUSC1B19A9TF2","",264,&os44_ymtc_eua_ops},
 {"XBSTOR","XBUSC1B2AA9TF3","",528,&os44_ymtc_eua_ops},
 {"XBSTOR","XBUSC1B5AAATH3","",528,&os44_ymtc_eua_ops},
};
struct os44_eua_check_record os44_eua_checklist[3]={
 {9,"XBUSC1B2AA9TF3",8},
 {10,"HN8T171EJKX151",8},
 {0,"",0},
};
