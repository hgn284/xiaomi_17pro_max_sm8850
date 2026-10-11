#include "ufsswap_query_reconstruction.h"
/* Stock mutable shared configs: revision quirk intentionally persists across calls. */
static struct os44_swap_config_view hynix_spec = {0x80,0x81,0x82,0x83,0x0,0x0,0x0,0x18,0x0};
static struct os44_swap_config_view samsung_spec = {0x9a,0x9b,0x95,0x96,0x0,0x93,0x1,0x18,0x0};
static struct os44_swap_config_view micron_spec = {0x84,0x85,0x86,0x87,0x0,0x0,0x0,0x18,0x18};
static struct os44_swap_config_view ymtc_spec = {0x45,0x46,0x43,0x44,0x0,0x3f,0x2,0x18,0x0};
struct model_row { char vendor[9], model[17], revision[5]; os44_u32 value; struct os44_swap_config_view *config; };
static const struct model_row models[] = {
 {"SKhynix","HN8T171EJKX151","",256,&hynix_spec},
 {"SKhynix","HN8T271EJKX152","",512,&hynix_spec},
 {"SKhynix","HN8T371ZJKX154","",1024,&hynix_spec},
 {"SKhynix","HN8T281EJKX158","",512,&hynix_spec},
 {"SAMSUNG","KLUEG4RHKF-F0H1","",256,&samsung_spec},
 {"SAMSUNG","KLUFG8RHKF-F0H1","",512,&samsung_spec},
 {"SAMSUNG","KLUGG8NHKB-F0H1","",1024,&samsung_spec},
 {"MICRON","MT256GBGBB2U41","",256,&micron_spec},
 {"MICRON","MT512GBGBB4U41","",512,&micron_spec},
 {"XBSTOR","XBUSC1B19A9TF2","",256,&ymtc_spec},
 {"XBSTOR","XBUSC1B2AA9TF3","",512,&ymtc_spec},
 {"XBSTOR","XBUSC1B5AAATH3","",512,&ymtc_spec},
};
static int prefix(const unsigned char *p, const char *s, unsigned int bound)
{
 unsigned int i;
 /* Immutable recovered table fields are all terminated within these bounds. */
 for (i=0;i<bound && s[i];i++) if (p[i] != (unsigned char)s[i]) return 0;
 return i < bound;
}
int os44_ufsswap_select_model(struct os44_swap_view *swap, const unsigned char inquiry28[28])
{
 unsigned int i;
 for(i=0;i<sizeof(models)/sizeof(models[0]);i++) {
  const struct model_row *m=&models[i];
  if (!prefix(inquiry28,m->vendor,9) || !prefix(inquiry28+8,m->model,17) || !prefix(inquiry28+24,m->revision,5)) continue;
  swap->config=m->config; swap->model_index=m->value;
  if(inquiry28[24]=='X' && inquiry28[25]=='1' && inquiry28[26]=='0' && inquiry28[27]=='3') swap->config->revision_quirk=0x18;
  return 0;
 }
 return -95;
}
