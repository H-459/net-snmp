/* This file was generated by mib2c and is intended for use as
   a mib module for the ucd-snmp snmpd agent. */


/* This should always be included first before anything else */
#include <config.h>

#include <sys/types.h>
#if HAVE_WINSOCK_H
#include <winsock.h>
#endif
#if HAVE_STDLIB_H
#include <stdlib.h>
#endif
#if HAVE_STRING_H
#include <string.h>
#else
#include <strings.h>
#endif
#if HAVE_NETDB_H
#include <netdb.h>
#endif

/* minimal include directives */
#include "mibincl.h"
#include "header_complex.h"
#include "snmpNotifyTable.h"
#include "snmp-tc.h"
#include "target/snmpTargetParamsEntry.h"
#include "target/snmpTargetAddrEntry.h"
#include "target/target.h"
#include "agent_callbacks.h"

SNMPCallback store_snmpNotifyTable;

/* 
 * snmpNotifyTable_variables_oid:
 *   this is the top level oid that we want to register under.  This
 *   is essentially a prefix, with the suffix appearing in the
 *   variable below.
 */


oid snmpNotifyTable_variables_oid[] = { 1,3,6,1,6,3,13,1,1 };


/* 
 * variable2 snmpNotifyTable_variables:
 *   this variable defines function callbacks and type return information 
 *   for the snmpNotifyTable mib section 
 */


struct variable2 snmpNotifyTable_variables[] = {
/*  magic number        , variable type , ro/rw , callback fn  , L, oidsuffix */
#define   SNMPNOTIFYTAG         4
  { SNMPNOTIFYTAG       , ASN_OCTET_STR , RWRITE, var_snmpNotifyTable, 2, { 1,2 } },
#define   SNMPNOTIFYTYPE        5
  { SNMPNOTIFYTYPE      , ASN_INTEGER   , RWRITE, var_snmpNotifyTable, 2, { 1,3 } },
#define   SNMPNOTIFYSTORAGETYPE  6
  { SNMPNOTIFYSTORAGETYPE, ASN_INTEGER   , RWRITE, var_snmpNotifyTable, 2, { 1,4 } },
#define   SNMPNOTIFYROWSTATUS   7
  { SNMPNOTIFYROWSTATUS , ASN_INTEGER   , RWRITE, var_snmpNotifyTable, 2, { 1,5 } },

};
/*    (L = length of the oidsuffix) */


/* global storage of our data, saved in and configured by header_complex() */
static struct header_complex_index *snmpNotifyTableStorage = NULL;

int
send_notifications(int major, int minor, void *serverarg, void *clientarg) {
    struct header_complex_index *hptr;
    struct snmpNotifyTable_data *nptr;
    struct snmp_session *sess, *sptr;
    struct snmp_pdu *template_pdu = (struct snmp_pdu *) serverarg;
    
    DEBUGMSGTL(("send_notifications","starting: pdu=%x, vars=%x\n",
                template_pdu, template_pdu->variables));

    for(hptr = snmpNotifyTableStorage; hptr; hptr = hptr->next) {
        nptr = (struct snmpNotifyTable_data *) hptr->data;
        if (nptr->snmpNotifyRowStatus != RS_ACTIVE)
            continue;
        if (!nptr->snmpNotifyTag)
            continue;

        sess = get_target_sessions(nptr->snmpNotifyTag, NULL, NULL);

        /* XXX: filter appropriately */
        
        for(sptr=sess; sptr; sptr = sptr->next) {
            if (sptr->version == SNMP_VERSION_1 &&
                minor == SNMPD_CALLBACK_SEND_TRAP1) {
                send_trap_to_sess(sptr, template_pdu);
            } else if (sptr->version != SNMP_VERSION_1 &&
                       minor == SNMPD_CALLBACK_SEND_TRAP2) {
                if (nptr->snmpNotifyType == SNMPNOTIFYTYPE_INFORM) {
                    template_pdu->command = SNMP_MSG_INFORM;
                } else {
                    template_pdu->command = SNMP_MSG_TRAP2;
                }
                send_trap_to_sess(sptr, template_pdu);
            }
        }
    }
    return 0;
}

#define MAX_ENTRIES 1024

int
notifyTable_register_notifications(int major, int minor,
                                   void *serverarg, void *clientarg) {
    struct targetAddrTable_struct *ptr;
    struct targetParamTable_struct *pptr;
    struct snmpNotifyTable_data *nptr;
    int i;
    char buf[SNMP_MAXBUF_SMALL];
    oid udpdomain[] = { 1,3,6,1,6,1,1 };
    int udpdomainlen = sizeof(udpdomain)/sizeof(oid);
#ifdef HAVE_GETHOSTBYNAME
    struct hostent *hp;
#endif

    struct agent_add_trap_args *args =
        (struct agent_add_trap_args *) serverarg;
    struct snmp_session *ss;
    int confirm;

    if (!args)
        return (0);

    ss = args->ss;
    if (!ss)
        return (0);

    confirm = args->confirm;

    /* XXX: START move target creation to target code */
    for(i=0; i < MAX_ENTRIES; i++) {
        sprintf(buf, "internal%d", i);
        if (get_addrForName(buf) == NULL && get_paramEntry(buf) == NULL)
            break;
    }
    if (i == MAX_ENTRIES) {
        snmp_log(LOG_ERR,
                 "Can't register new trap destination: max limit reached: %d",
                 MAX_ENTRIES);
        snmp_sess_close(ss);
        return(0);
    }

    /* address */
    ptr = snmpTargetAddrTable_create();
    ptr->name = strdup(buf);
    memcpy(ptr->tDomain, udpdomain, udpdomainlen*sizeof(oid));
    ptr->tDomainLen = udpdomainlen;

#ifdef HAVE_GETHOSTBYNAME
    hp = gethostbyname(ss->peername);
    if (hp != NULL){
        /* XXX: fix for other domain types */
        ptr->tAddressLen = hp->h_length + 2;
        ptr->tAddress = malloc(ptr->tAddressLen);
        memmove(ptr->tAddress, hp->h_addr, hp->h_length);
        ptr->tAddress[hp->h_length] = (ss->remote_port & 0xff00) >> 8;
        ptr->tAddress[hp->h_length+1] = (ss->remote_port & 0xff);
    } else {
#endif /* HAVE_GETHOSTBYNAME */
        ptr->tAddressLen = 6;
        ptr->tAddress = (u_char *)calloc(1, ptr->tAddressLen);
#ifdef HAVE_GETHOSTBYNAME
    }
#endif /* HAVE_GETHOSTBYNAME */
    ptr->timeout = ss->timeout/1000;
    ptr->retryCount = ss->retries;
    ptr->tagList = strdup(ptr->name);
    ptr->params = strdup(ptr->name);
    ptr->storageType = ST_READONLY;
    ptr->rowStatus = RS_ACTIVE;
    ptr->sess = ss;
    DEBUGMSGTL(("trapsess", "adding to trap table\n"));
    snmpTargetAddrTable_add(ptr);

    /* param */
    pptr = snmpTargetParamTable_create();
    pptr->paramName = strdup(buf);
    pptr->mpModel = ss->version;
    if (ss->version == SNMP_VERSION_3) {
        pptr->secModel = ss->securityModel;
        pptr->secLevel = ss->securityLevel;
        pptr->secName = (char *)malloc(ss->securityNameLen+1);
        memcpy((void *) pptr->secName, (void *) ss->securityName,
               ss->securityNameLen);
        pptr->secName[ss->securityNameLen] = 0;
    } else {
        pptr->secModel = ss->version == SNMP_VERSION_1 ?
            SNMP_SEC_MODEL_SNMPv1 : SNMP_SEC_MODEL_SNMPv2c;
        pptr->secLevel = SNMP_SEC_LEVEL_NOAUTH;
        pptr->secName = NULL;
        if (ss->community && (ss->community_len > 0)) {
            pptr->secName = (char *)malloc(ss->community_len+1);
            memcpy((void *) pptr->secName, (void *) ss->community,
                   ss->community_len);
            pptr->secName[ss->community_len] = 0;
        }
    }
    pptr->storageType = ST_READONLY;
    pptr->rowStatus = RS_ACTIVE;
    snmpTargetParamTable_add(pptr);
    /* XXX: END move target creation to target code */
            
    /* notify table */
    nptr = SNMP_MALLOC_STRUCT(snmpNotifyTable_data);
    nptr->snmpNotifyName = strdup(buf);
    nptr->snmpNotifyNameLen = strlen(buf);
    nptr->snmpNotifyTag = strdup(buf);
    nptr->snmpNotifyTagLen = strlen(buf);
    nptr->snmpNotifyType = confirm ?
        SNMPNOTIFYTYPE_TRAP : SNMPNOTIFYTYPE_INFORM;
    nptr->snmpNotifyStorageType = ST_READONLY;
    nptr->snmpNotifyRowStatus = RS_ACTIVE;

    snmpNotifyTable_add(nptr);
    return 0;
}

/*
 * init_snmpNotifyTable():
 *   Initialization routine.  This is called when the agent starts up.
 *   At a minimum, registration of your variables should take place here.
 */
void init_snmpNotifyTable(void) {
  DEBUGMSGTL(("snmpNotifyTable", "initializing...  "));


  /* register ourselves with the agent to handle our mib tree */
  REGISTER_MIB("snmpNotifyTable", snmpNotifyTable_variables, variable2,
               snmpNotifyTable_variables_oid);


  /* register our config handler(s) to deal with registrations */
  snmpd_register_config_handler("snmpNotifyTable", parse_snmpNotifyTable, NULL,
                                NULL);


  /* we need to be called back later to store our data */
  snmp_register_callback(SNMP_CALLBACK_LIBRARY, SNMP_CALLBACK_STORE_DATA,
                         store_snmpNotifyTable, NULL);

  snmp_register_callback(SNMP_CALLBACK_APPLICATION, SNMPD_CALLBACK_SEND_TRAP1,
                         send_notifications, NULL);
  snmp_register_callback(SNMP_CALLBACK_APPLICATION, SNMPD_CALLBACK_SEND_TRAP2,
                         send_notifications, NULL);
  snmp_register_callback(SNMP_CALLBACK_APPLICATION,
                         SNMPD_CALLBACK_REGISTER_NOTIFICATIONS,
                         notifyTable_register_notifications, NULL);

  /* place any other initialization junk you need here */


  DEBUGMSGTL(("snmpNotifyTable", "done.\n"));
}


/* 
 * snmpNotifyTable_add(): adds a structure node to our data set 
 */
int
snmpNotifyTable_add(struct snmpNotifyTable_data *thedata) {
  struct variable_list *vars = NULL;


  DEBUGMSGTL(("snmpNotifyTable", "adding data...  "));
 /* add the index variables to the varbind list, which is 
    used by header_complex to index the data */


  snmp_varlist_add_variable(&vars, NULL, 0, ASN_PRIV_IMPLIED_OCTET_STR, (u_char *) thedata->snmpNotifyName, thedata->snmpNotifyNameLen); /* snmpNotifyName */



  header_complex_add_data(&snmpNotifyTableStorage, vars, thedata);
  DEBUGMSGTL(("snmpNotifyTable","registered an entry\n"));


  DEBUGMSGTL(("snmpNotifyTable", "done.\n"));
  return SNMPERR_SUCCESS;
}


/*
 * parse_snmpNotifyTable():
 *   parses .conf file entries needed to configure the mib.
 */
void
parse_snmpNotifyTable(const char *token, char *line) {
  size_t tmpint;
  struct snmpNotifyTable_data *StorageTmp = SNMP_MALLOC_STRUCT(snmpNotifyTable_data);


    DEBUGMSGTL(("snmpNotifyTable", "parsing config...  "));


  if (StorageTmp == NULL) {
    config_perror("malloc failure");
    return;
  }

  line = read_config_read_data(ASN_OCTET_STR, line, &StorageTmp->snmpNotifyName, &StorageTmp->snmpNotifyNameLen);
  if (StorageTmp->snmpNotifyName == NULL) {
    config_perror("invalid specification for snmpNotifyName");
    return;
  }

  line = read_config_read_data(ASN_OCTET_STR, line, &StorageTmp->snmpNotifyTag, &StorageTmp->snmpNotifyTagLen);
  if (StorageTmp->snmpNotifyTag == NULL) {
    config_perror("invalid specification for snmpNotifyTag");
    return;
  }
  
  line = read_config_read_data(ASN_INTEGER, line, &StorageTmp->snmpNotifyType, &tmpint);

  line = read_config_read_data(ASN_INTEGER, line, &StorageTmp->snmpNotifyStorageType, &tmpint);

  line = read_config_read_data(ASN_INTEGER, line, &StorageTmp->snmpNotifyRowStatus, &tmpint);




  snmpNotifyTable_add(StorageTmp);
    

  DEBUGMSGTL(("snmpNotifyTable", "done.\n"));
}




/*
 * store_snmpNotifyTable():
 *   stores .conf file entries needed to configure the mib.
 */
int
store_snmpNotifyTable(int majorID, int minorID, void *serverarg,
                      void *clientarg) {
  char line[SNMP_MAXBUF];
  char *cptr;
  size_t tmpint;
  struct snmpNotifyTable_data *StorageTmp;
  struct header_complex_index *hcindex;


  DEBUGMSGTL(("snmpNotifyTable", "storing data...  "));


  for(hcindex=snmpNotifyTableStorage; hcindex != NULL; 
      hcindex = hcindex->next) {
    StorageTmp = (struct snmpNotifyTable_data *) hcindex->data;

    if (StorageTmp->snmpNotifyStorageType == ST_NONVOLATILE) {

        memset(line,0,sizeof(line));
        strcat(line, "snmpNotifyTable ");
        cptr = line + strlen(line);

        cptr = read_config_store_data(ASN_OCTET_STR, cptr, &StorageTmp->snmpNotifyName, &StorageTmp->snmpNotifyNameLen);
        cptr = read_config_store_data(ASN_OCTET_STR, cptr, &StorageTmp->snmpNotifyTag, &StorageTmp->snmpNotifyTagLen);
        cptr = read_config_store_data(ASN_INTEGER, cptr, &StorageTmp->snmpNotifyType, &tmpint);
        cptr = read_config_store_data(ASN_INTEGER, cptr, &StorageTmp->snmpNotifyStorageType, &tmpint);
        cptr = read_config_store_data(ASN_INTEGER, cptr, &StorageTmp->snmpNotifyRowStatus, &tmpint);

        snmpd_store_config(line);
    }
  }
  DEBUGMSGTL(("snmpNotifyTable", "done.\n"));
  return 0;
}




/*
 * var_snmpNotifyTable():
 *   Handle this table separately from the scalar value case.
 *   The workings of this are basically the same as for var_snmpNotifyTable above.
 */
unsigned char *
var_snmpNotifyTable(struct variable *vp,
    	    oid     *name,
    	    size_t  *length,
    	    int     exact,
    	    size_t  *var_len,
    	    WriteMethod **write_method)
{


struct snmpNotifyTable_data *StorageTmp = NULL;


  DEBUGMSGTL(("snmpNotifyTable", "var_snmpNotifyTable: Entering...  \n"));
  /* 
   * this assumes you have registered all your data properly
   */
  if ((StorageTmp = (struct snmpNotifyTable_data *)
       header_complex(snmpNotifyTableStorage, vp,name,length,exact,
                      var_len,write_method)) == NULL) {
      DEBUGMSGTL(("snmpNotifyTable", "no row: magic=%d...  \n", vp->magic));
      if (vp->magic == SNMPNOTIFYROWSTATUS) {
          DEBUGMSGTL(("snmpNotifyTable", "var_snmpNotifyTable: create?...  \n"));
          *write_method = write_snmpNotifyRowStatus;
      }
      return NULL;
  }


  /* 
   * this is where we do the value assignments for the mib results.
   */
  switch(vp->magic) {


    case SNMPNOTIFYTAG:
        *write_method = write_snmpNotifyTag;
        *var_len = StorageTmp->snmpNotifyTagLen;
        return (u_char *) StorageTmp->snmpNotifyTag;

    case SNMPNOTIFYTYPE:
        *write_method = write_snmpNotifyType;
        *var_len = sizeof(StorageTmp->snmpNotifyType);
        return (u_char *) &StorageTmp->snmpNotifyType;

    case SNMPNOTIFYSTORAGETYPE:
        *write_method = write_snmpNotifyStorageType;
        *var_len = sizeof(StorageTmp->snmpNotifyStorageType);
        return (u_char *) &StorageTmp->snmpNotifyStorageType;

    case SNMPNOTIFYROWSTATUS:
        *write_method = write_snmpNotifyRowStatus;
        *var_len = sizeof(StorageTmp->snmpNotifyRowStatus);
        return (u_char *) &StorageTmp->snmpNotifyRowStatus;


    default:
      ERROR_MSG("");
  }
  return NULL;
}




int
write_snmpNotifyTag(int      action,
            u_char   *var_val,
            u_char   var_val_type,
            size_t   var_val_len,
            u_char   *statP,
            oid      *name,
            size_t    name_len)
{
  static char * tmpvar;
  struct snmpNotifyTable_data *StorageTmp = NULL;
  static size_t tmplen;
  size_t newlen=name_len - (sizeof(snmpNotifyTable_variables_oid)/sizeof(oid) + 3 - 1);


  DEBUGMSGTL(("snmpNotifyTable", "write_snmpNotifyTag entering action=%d...  \n", action));
  if ((StorageTmp = (struct snmpNotifyTable_data *)
       header_complex(snmpNotifyTableStorage, NULL,
                      &name[sizeof(snmpNotifyTable_variables_oid)/sizeof(oid) + 3 - 1], 
                      &newlen, 1, NULL, NULL)) == NULL)
      return SNMP_ERR_NOSUCHNAME; /* remove if you support creation here */


  switch ( action ) {
        case RESERVE1:
          if (var_val_type != ASN_OCTET_STR){
              fprintf(stderr, "write to snmpNotifyTag not ASN_OCTET_STR\n");
              return SNMP_ERR_WRONGTYPE;
          }
          break;


        case RESERVE2:
             /* memory reseveration, final preparation... */
          break;


        case FREE:
             /* Release any resources that have been allocated */
          break;


        case ACTION:
             /* The variable has been stored in string for
             you to use, and you have just been asked to do something with
             it.  Note that anything done here must be reversable in the UNDO case */
             tmpvar = StorageTmp->snmpNotifyTag;
             tmplen = StorageTmp->snmpNotifyTagLen;
             memdup((u_char **) &StorageTmp->snmpNotifyTag, var_val, var_val_len);
             StorageTmp->snmpNotifyTagLen = var_val_len;
          break;


        case UNDO:
             /* Back out any changes made in the ACTION case */
             SNMP_FREE(StorageTmp->snmpNotifyTag);
             StorageTmp->snmpNotifyTag = tmpvar;
             StorageTmp->snmpNotifyTagLen = tmplen;
	     tmpvar = NULL;
          break;


        case COMMIT:
             /* Things are working well, so it's now safe to make the change
             permanently.  Make sure that anything done here can't fail! */
	     SNMP_FREE(tmpvar);
          break;
  }
  return SNMP_ERR_NOERROR;
}



int
write_snmpNotifyType(int      action,
            u_char   *var_val,
            u_char   var_val_type,
            size_t   var_val_len,
            u_char   *statP,
            oid      *name,
            size_t    name_len)
{
  static int tmpvar;
  struct snmpNotifyTable_data *StorageTmp = NULL;
  size_t newlen=name_len - (sizeof(snmpNotifyTable_variables_oid)/sizeof(oid) + 3 - 1);


  DEBUGMSGTL(("snmpNotifyTable", "write_snmpNotifyType entering action=%d...  \n", action));
  if ((StorageTmp = (struct snmpNotifyTable_data *)
       header_complex(snmpNotifyTableStorage, NULL,
                      &name[sizeof(snmpNotifyTable_variables_oid)/sizeof(oid) + 3 - 1], 
                      &newlen, 1, NULL, NULL)) == NULL)
      return SNMP_ERR_NOSUCHNAME; /* remove if you support creation here */


  switch ( action ) {
        case RESERVE1:
          if (var_val_type != ASN_INTEGER){
              fprintf(stderr, "write to snmpNotifyType not ASN_INTEGER\n");
              return SNMP_ERR_WRONGTYPE;
          }
          break;


        case RESERVE2:
             /* memory reseveration, final preparation... */
          break;


        case FREE:
             /* Release any resources that have been allocated */
          break;


        case ACTION:
             /* The variable has been stored in long_ret for
             you to use, and you have just been asked to do something with
             it.  Note that anything done here must be reversable in the UNDO case */
             tmpvar = StorageTmp->snmpNotifyType;
             StorageTmp->snmpNotifyType = *((long *) var_val);
          break;


        case UNDO:
             /* Back out any changes made in the ACTION case */
             StorageTmp->snmpNotifyType = tmpvar;
          break;


        case COMMIT:
             /* Things are working well, so it's now safe to make the change
             permanently.  Make sure that anything done here can't fail! */

          break;
  }
  return SNMP_ERR_NOERROR;
}



int
write_snmpNotifyStorageType(int      action,
            u_char   *var_val,
            u_char   var_val_type,
            size_t   var_val_len,
            u_char   *statP,
            oid      *name,
            size_t    name_len)
{
  static int tmpvar;
  struct snmpNotifyTable_data *StorageTmp = NULL;
  size_t newlen=name_len - (sizeof(snmpNotifyTable_variables_oid)/sizeof(oid) + 3 - 1);


  DEBUGMSGTL(("snmpNotifyTable", "write_snmpNotifyStorageType entering action=%d...  \n", action));
  if ((StorageTmp = (struct snmpNotifyTable_data *)
       header_complex(snmpNotifyTableStorage, NULL,
                      &name[sizeof(snmpNotifyTable_variables_oid)/sizeof(oid) + 3 - 1], 
                      &newlen, 1, NULL, NULL)) == NULL)
      return SNMP_ERR_NOSUCHNAME; /* remove if you support creation here */


  switch ( action ) {
        case RESERVE1:
          if (var_val_type != ASN_INTEGER){
              fprintf(stderr, "write to snmpNotifyStorageType not ASN_INTEGER\n");
              return SNMP_ERR_WRONGTYPE;
          }
          break;


        case RESERVE2:
             /* memory reseveration, final preparation... */
          break;


        case FREE:
             /* Release any resources that have been allocated */
          break;


        case ACTION:
             /* The variable has been stored in long_ret for
             you to use, and you have just been asked to do something with
             it.  Note that anything done here must be reversable in the UNDO case */
             tmpvar = StorageTmp->snmpNotifyStorageType;
             StorageTmp->snmpNotifyStorageType = *((long *) var_val);
          break;


        case UNDO:
             /* Back out any changes made in the ACTION case */
             StorageTmp->snmpNotifyStorageType = tmpvar;
          break;


        case COMMIT:
             /* Things are working well, so it's now safe to make the change
             permanently.  Make sure that anything done here can't fail! */

          break;
  }
  return SNMP_ERR_NOERROR;
}






int
write_snmpNotifyRowStatus(int      action,
            u_char   *var_val,
            u_char   var_val_type,
            size_t   var_val_len,
            u_char   *statP,
            oid      *name,
            size_t    name_len)
{
  struct snmpNotifyTable_data *StorageTmp = NULL;
  static struct snmpNotifyTable_data *StorageNew, *StorageDel;
  size_t newlen=name_len - (sizeof(snmpNotifyTable_variables_oid)/sizeof(oid) + 3 - 1);
  static int old_value;
  int set_value;
  static struct variable_list *vars, *vp;
  struct header_complex_index *hciptr;


  DEBUGMSGTL(("snmpNotifyTable", "write_snmpNotifyRowStatus entering action=%d...  \n", action));
  StorageTmp = (struct snmpNotifyTable_data *)
    header_complex(snmpNotifyTableStorage, NULL,
                   &name[sizeof(snmpNotifyTable_variables_oid)/sizeof(oid) + 3 - 1], 
                   &newlen, 1, NULL, NULL);
  

  

  if (var_val_type != ASN_INTEGER || var_val == NULL){
    fprintf(stderr, "write to snmpNotifyRowStatus not ASN_INTEGER\n");
    return SNMP_ERR_WRONGTYPE;
  }
  set_value = *((long *) var_val);


  /* check legal range, and notReady is reserved for us, not a user */
  if (set_value < 1 || set_value > 6 || set_value == RS_NOTREADY)
    return SNMP_ERR_INCONSISTENTVALUE;
    

  switch ( action ) {
        case RESERVE1:
  /* stage one: test validity */
          if (StorageTmp == NULL) {
            /* create the row now? */


            /* ditch illegal values now */
            if (set_value == RS_ACTIVE || set_value == RS_NOTINSERVICE)
              return SNMP_ERR_INCONSISTENTVALUE;
    

            /* destroying a non-existent row is actually legal */
            if (set_value == RS_DESTROY) {
              return SNMP_ERR_NOERROR;
            }
          } else {
            /* row exists.  Check for a valid state change */
            if (set_value == RS_CREATEANDGO || set_value == RS_CREATEANDWAIT) {
              /* can't create a row that exists */
              return SNMP_ERR_INCONSISTENTVALUE;
            }
    /* XXX: interaction with row storage type needed */
          }
          break;




        case RESERVE2:
          /* memory reseveration, final preparation... */
          if (StorageTmp == NULL) {
            /* creation */
            vars = NULL;

            snmp_varlist_add_variable(&vars, NULL, 0, ASN_PRIV_IMPLIED_OCTET_STR, NULL, 0); /* snmpNotifyName */

            if (header_complex_parse_oid(&(name[sizeof(snmpNotifyTable_variables_oid)/sizeof(oid)+2]), newlen,
                                         vars) != SNMPERR_SUCCESS) {
              /* XXX: free, zero vars */
              snmp_free_var(vars);
              return SNMP_ERR_INCONSISTENTNAME;
            }
            vp = vars;


            StorageNew = SNMP_MALLOC_STRUCT(snmpNotifyTable_data);
            memdup((u_char **) &(StorageNew->snmpNotifyName), 
                   vp->val.string,
                   vp->val_len);
            StorageNew->snmpNotifyNameLen = vp->val_len;
            vp = vp->next_variable;


            /* default values */
            StorageNew->snmpNotifyStorageType = ST_NONVOLATILE;
            StorageNew->snmpNotifyType = SNMPNOTIFYTYPE_TRAP;
            StorageNew->snmpNotifyTagLen = 0;
            StorageNew->snmpNotifyTag = (char *)malloc(1); /* bogus pointer */

            StorageNew->snmpNotifyRowStatus = set_value;
            snmp_free_var(vars);
          }
          

          break;




        case FREE:
          /* XXX: free, zero vars */
          /* Release any resources that have been allocated */
          break;




        case ACTION:
             /* The variable has been stored in set_value for you to
             use, and you have just been asked to do something with
             it.  Note that anything done here must be reversable in
             the UNDO case */
             

             if (StorageTmp == NULL) {
               /* row creation, so add it */
               if (StorageNew != NULL)
                 snmpNotifyTable_add(StorageNew);
               /* XXX: ack, and if it is NULL? */
             } else if (set_value != RS_DESTROY) {
               /* set the flag? */
               old_value = StorageTmp->snmpNotifyRowStatus;
               StorageTmp->snmpNotifyRowStatus = *((long *) var_val);
             } else {
               /* destroy...  extract it for now */
               hciptr =
                 header_complex_find_entry(snmpNotifyTableStorage,
                                           StorageTmp);
               StorageDel = (struct snmpNotifyTable_data *)
                 header_complex_extract_entry(&snmpNotifyTableStorage,
                                              hciptr);
             }
          break;




        case UNDO:
             /* Back out any changes made in the ACTION case */
             if (StorageTmp == NULL) {
               /* row creation, so remove it again */
               hciptr =
                 header_complex_find_entry(snmpNotifyTableStorage,
                                           StorageTmp);
               StorageDel = (struct snmpNotifyTable_data *)
                 header_complex_extract_entry(&snmpNotifyTableStorage,
                                              hciptr);
               /* XXX: free it */
             } else if (StorageDel != NULL) {
               /* row deletion, so add it again */
               snmpNotifyTable_add(StorageDel);
             } else {
               StorageTmp->snmpNotifyRowStatus = old_value;
             }
          break;




        case COMMIT:
             /* Things are working well, so it's now safe to make the change
             permanently.  Make sure that anything done here can't fail! */
          if (StorageDel != NULL) {
            StorageDel = NULL;
            /* XXX: free it, its dead */
          }
          if (StorageTmp && StorageTmp->snmpNotifyRowStatus == RS_CREATEANDGO) {
              StorageTmp->snmpNotifyRowStatus = RS_ACTIVE;
          } else if (StorageTmp &&
                     StorageTmp->snmpNotifyRowStatus == RS_CREATEANDWAIT) {
              StorageTmp->snmpNotifyRowStatus = RS_NOTINSERVICE;
          }
          break;
  }
  return SNMP_ERR_NOERROR;
}
