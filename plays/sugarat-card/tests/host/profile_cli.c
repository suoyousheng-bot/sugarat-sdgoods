/* Copyright (c) 2026 Deotaland. SPDX-License-Identifier: Apache-2.0 */
/** Browser fixture adapter: the actual parser and file store, without a device network. */
#include "card_store.h"
#include <stdio.h>
#include <string.h>
int main(int argc,char **argv)
{
    if(argc!=3)return 2;card_store_t store;card_state_t state={0};uint8_t id[16]={1};
    if(!card_store_files_open(&store,argv[2]))return 3;
    card_result_t result=card_store_load(&store,&state,id);
    if(result==CARD_OK&&!strcmp(argv[1],"save")) {
        char bytes[CARD_JSON_MAX+1];size_t size=fread(bytes,1,sizeof(bytes),stdin);
        if(card_profile_parse(bytes,size,&state.profile)!=CARD_OK)result=CARD_INVALID;
        else result=card_store_save(&store,&state);
    }
    if(result==CARD_OK){char bytes[CARD_JSON_MAX];if(card_profile_json(&state.profile,bytes,sizeof(bytes)))puts(bytes);else result=CARD_IO;}
    card_store_close(&store);return result==CARD_OK?0:4;
}
