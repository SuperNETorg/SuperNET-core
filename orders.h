//
//  orders.h
//
//  Created by jl777 on 7/9/14.
//  Copyright (c) 2014 jl777. All rights reserved.
//

#ifndef xcode_orders_h
#define xcode_orders_h

#define INSTANTDEX_NAME "iDEX"
#define INSTANTDEX_NATIVE 0
#define INSTANTDEX_ASSET 1
#define INSTANTDEX_MSCOIN 2
#define INSTANTDEX_UNKNOWN 3
#define ORDERBOOK_EXPIRATION 300
#define _obookid(baseid,relid) ((baseid) ^ (relid))

char *assetmap[][3] =
{
    { "5527630", "NXT", "8" },
    { "17554243582654188572", "BTC", "8" },
    { "4551058913252105307", "BTC", "8" },
    { "12659653638116877017", "BTC", "8" },
    { "11060861818140490423", "BTCD", "4" },
    { "6918149200730574743", "BTCD", "4" },
    { "13120372057981370228", "BITS", "6" },
    { "2303962892272487643", "DOGE", "4" },
    { "16344939950195952527", "DOGE", "4" },
    { "6775076774325697454", "OPAL", "8" },
    { "7734432159113182240", "VPN", "4" },
    { "9037144112883608562", "VRC", "8" },
    { "1369181773544917037", "BBR", "8" },
    { "17353118525598940144", "DRK", "8" },
    { "2881764795164526882", "LTC", "8" },
    { "7117580438310874759", "BC", "4" },
    { "275548135983837356", "VIA", "4" },
};

struct rambook_info
{
    UT_hash_handle hh;
    char url[128],base[16],rel[16],lbase[16],lrel[16],exchange[16];
    struct InstantDEX_quote *quotes;
    uint64_t assetids[4],basemult,relmult;
    int32_t numquotes,maxquotes;
} *Rambooks;

uint64_t _calc_decimals_mult(int32_t decimals)
{
    int32_t i;
    uint64_t mult = 1;
    for (i=7-decimals; i>=0; i--)
        mult *= 10;
    return(mult);
}

int32_t _set_assetname(uint64_t *multp,char *buf,char *jsonstr)
{
    int32_t decimals = -1;
    cJSON *json;
    *multp = 0;
    if ( (json= cJSON_Parse(jsonstr)) != 0 )
    {
        if ( get_cJSON_int(json,"errorCode") == 0 )
        {
            decimals = (int32_t)get_cJSON_int(json,"decimals");
            *multp = _calc_decimals_mult(decimals);
            if ( extract_cJSON_str(buf,sizeof(buf),json,"name") <= 0 )
                decimals = -1;
        }
        free_json(json);
    }
    return(decimals);
}

uint32_t set_assetname(uint64_t *multp,char *name,uint64_t assetbits)
{
    char assetstr[64],buf[1024],*jsonstr,*jsonstr2;
    uint32_t i,retval = INSTANTDEX_UNKNOWN;
    *multp = 1;
    expand_nxt64bits(assetstr,assetbits);
    for (i=0; i<(int32_t)(sizeof(assetmap)/sizeof(*assetmap)); i++)
    {
        if ( strcmp(assetmap[i][0],assetstr) == 0 )
        {
            strcpy(name,assetmap[i][1]);
            *multp = atoi(assetmap[i][2]);
            return(INSTANTDEX_NATIVE); // native crypto type
        }
    }
    if ( (jsonstr= issue_getAsset(0,assetstr)) != 0 )
    {
        if ( _set_assetname(multp,buf,jsonstr) < 0 )
        {
            if ( (jsonstr2= issue_getAsset(1,assetstr)) != 0 )
            {
                if ( _set_assetname(multp,buf,jsonstr) >= 0 )
                {
                    retval = INSTANTDEX_MSCOIN;
                    strncpy(name,buf,15);
                    name[15] = 0;
                }
                free(jsonstr2);
            }
        }
        else
        {
            retval = INSTANTDEX_ASSET;
            strncpy(name,buf,15);
            name[15] = 0;
        }
        free(jsonstr);
    }
    return(retval);
}

cJSON *rambook_json(struct rambook_info *rb)
{
    cJSON *json = cJSON_CreateObject();
    char numstr[64];
    cJSON_AddItemToObject(json,"base",cJSON_CreateString(rb->base));
    sprintf(numstr,"%llu",(long long)rb->assetids[0]), cJSON_AddItemToObject(json,"baseid",cJSON_CreateString(numstr));
    cJSON_AddItemToObject(json,"rel",cJSON_CreateString(rb->rel));
    sprintf(numstr,"%llu",(long long)rb->assetids[1]), cJSON_AddItemToObject(json,"relid",cJSON_CreateString(numstr));
    cJSON_AddItemToObject(json,"numquotes",cJSON_CreateNumber(rb->numquotes));
    sprintf(numstr,"%llu",(long long)rb->assetids[3]), cJSON_AddItemToObject(json,"type",cJSON_CreateString(numstr));
    memset(numstr,0,sizeof(numstr)), strcpy(numstr,(char *)&rb->assetids[2]), cJSON_AddItemToObject(json,"exchange",cJSON_CreateString(numstr));
    return(json);
}

uint64_t purge_oldest_order(struct rambook_info *rb,struct InstantDEX_quote *iQ) // allow one pair per orderbook
{
    char NXTaddr[64];
    struct NXT_acct *np;
    int32_t age,oldi,createdflag;
    uint64_t nxt64bits = 0;
    uint32_t now,i,oldest = 0;
    if ( rb->numquotes == 0 )
        return(0);
    oldi = -1;
    now = (uint32_t)time(NULL);
    for (i=0; i<rb->numquotes; i++)
    {
        age = (now - rb->quotes[i].timestamp);
        if ( age >= ORDERBOOK_EXPIRATION )
        {
            if ( (iQ == 0 || rb->quotes[i].nxt64bits == iQ->nxt64bits) && (oldest == 0 || rb->quotes[i].timestamp < oldest) )
            {
                oldest = rb->quotes[i].timestamp;
                //fprintf(stderr,"(oldi.%d %u) ",j,oldest);
                nxt64bits = rb->quotes[i].nxt64bits;
                oldi = i;
            }
        }
    }
    if ( oldi >= 0 )
    {
        rb->quotes[oldi] = rb->quotes[--rb->numquotes];
        memset(&rb->quotes[rb->numquotes],0,sizeof(rb->quotes[rb->numquotes]));
        expand_nxt64bits(NXTaddr,nxt64bits);
        np = get_NXTacct(&createdflag,Global_mp,NXTaddr);
        if ( np->openorders > 0 )
            np->openorders--;
        fprintf(stderr,"purge_oldest_order from NXT.%llu (openorders.%d) oldi.%d timestamp %u\n",(long long)nxt64bits,np->openorders,oldi,oldest);
    } else fprintf(stderr,"no purges: numquotes.%d\n",rb->numquotes);
    return(nxt64bits);
}

uint64_t stringbits(char *str)
{
    uint64_t bits = 0;
    char buf[9];
    int32_t i;
    memset(buf,0,sizeof(buf));
    for (i=0; i<8; i++)
        if ( (buf[i]= str[i]) == 0 )
            break;
    memcpy(&bits,buf,sizeof(bits));
    return(bits);
}

struct rambook_info *find_rambook(uint64_t rambook_hashbits[4])
{
    struct rambook_info *rb;
    HASH_FIND(hh,Rambooks,rambook_hashbits,sizeof(rb->assetids),rb);
    return(rb);
}

struct rambook_info *get_rambook(uint64_t baseid,uint64_t relid,char *exchange)
{
    char base[16],rel[16];
    uint64_t assetids[4],basemult,relmult,exchangebits;
    uint32_t i,basetype,reltype;
    struct rambook_info *rb;
    exchangebits = stringbits(exchange);
    basetype = set_assetname(&basemult,base,baseid);
    reltype = set_assetname(&relmult,rel,relid);
    printf("get_rambook.(%s) %s %llu.%d / %s %llu.%d [%llu %llu]\n",exchange,base,(long long)baseid,basetype,rel,(long long)relid,reltype,(long long)basemult,(long long)relmult);
    assetids[0] = baseid, assetids[1] = relid, assetids[2] = exchangebits, assetids[3] = (((uint64_t)basetype << 32) | reltype);
    if ( (rb= find_rambook(assetids)) == 0 )
    {
        rb = calloc(1,sizeof(*rb));
        strncpy(rb->exchange,exchange,sizeof(rb->exchange)-1);
        strcpy(rb->base,base), strcpy(rb->rel,rel);
        rb->basemult = basemult, rb->relmult = relmult;
        touppercase(rb->base), strcpy(rb->lbase,rb->base), tolowercase(rb->lbase);
        touppercase(rb->rel), strcpy(rb->lrel,rb->rel), tolowercase(rb->lrel);
        for (i=0; i<4; i++)
            rb->assetids[i] = assetids[i];
        printf("CREATE RAMBOOK.(%llu.%d -> %llu.%d) %s\n",(long long)baseid,basetype,(long long)relid,reltype,exchange);
        HASH_ADD(hh,Rambooks,assetids,sizeof(rb->assetids),rb);
    }
    purge_oldest_order(rb,0);
    return(rb);
}

struct rambook_info **get_allrambooks(int32_t *numbooksp)
{
    int32_t i = 0;
    struct rambook_info *rb,*tmp,**obooks;
    *numbooksp = HASH_COUNT(Rambooks);
    obooks = calloc(*numbooksp,sizeof(*rb));
    HASH_ITER(hh,Rambooks,rb,tmp)
    {
        purge_oldest_order(rb,0);
        obooks[i++] = rb;
        printf("rambook.(%s) %s %llu.%d / %s %llu.%d\n",rb->exchange,rb->base,(long long)rb->assetids[0],(int)(rb->assetids[3]>>32),rb->rel,(long long)rb->assetids[1],(uint32_t)rb->assetids[3]);
    }
    if ( i != *numbooksp )
        printf("get_allrambooks HASH_COUNT.%d vs i.%d\n",*numbooksp,i);
    return(obooks);
}

cJSON *all_orderbooks()
{
    cJSON *array,*json = 0;
    struct rambook_info **obooks;
    int32_t i,numbooks;
    if ( (obooks= get_allrambooks(&numbooks)) != 0 )
    {
        array = cJSON_CreateArray();
        for (i=0; i<numbooks; i++)
            cJSON_AddItemToArray(array,rambook_json(obooks[i]));
        free(obooks);
        json = cJSON_CreateObject();
        cJSON_AddItemToObject(json,"orderbooks",array);
    }
    return(json);
}

uint64_t find_best_market_maker() // store ranked list
{
    char cmdstr[1024],NXTaddr[64],receiverstr[MAX_JSON_FIELD],*jsonstr;
    cJSON *json,*array,*txobj;
    int32_t i,n,createdflag;
    struct NXT_acct *np,*maxnp = 0;
    uint64_t amount,senderbits;
    uint32_t now = (uint32_t)time(NULL);
    sprintf(cmdstr,"requestType=getAccountTransactions&account=%s&timestamp=%u&type=0&subtype=0",INSTANTDEX_ACCT,38785003);
    if ( (jsonstr= bitcoind_RPC(0,"curl",NXTAPIURL,0,0,cmdstr)) != 0 )
    {
       // mm string.({"requestProcessingTime":33,"transactions":[{"fullHash":"2a2aab3b84dadf092cf4cedcd58a8b5a436968e836338e361c45651bce0ef97e","confirmations":203,"signatureHash":"52a4a43d9055fe4861b3d13fbd03a42fecb8c9ad4ac06a54da7806a8acd9c5d1","transaction":"711527527619439146","amountNQT":"1100000000","transactionIndex":2,"ecBlockHeight":360943,"block":"6797727125503999830","recipientRS":"NXT-74VC-NKPE-RYCA-5LMPT","type":0,"feeNQT":"100000000","recipient":"4383817337783094122","version":1,"sender":"423766016895692955","timestamp":38929220,"ecBlockId":"10121077683890606382","height":360949,"subtype":0,"senderPublicKey":"4e5bbad625df3d536fa90b1e6a28c3f5a56e1fcbe34132391c8d3fd7f671cb19","deadline":1440,"blockTimestamp":38929430,"senderRS":"NXT-8E6V-YBWH-5VMR-26ESD","signature":"4318f36d9cf68ef0a8f58303beb0ed836b670914065a868053da5fe8b096bc0c268e682c0274e1614fc26f81be4564ca517d922deccf169eafa249a88de58036"}]})
        if ( (json= cJSON_Parse(jsonstr)) != 0 )
        {
            if ( (array= cJSON_GetObjectItem(json,"transactions")) != 0 && is_cJSON_Array(array) != 0 && (n= cJSON_GetArraySize(array)) > 0 )
            {
                for (i=0; i<n; i++)
                {
                    txobj = cJSON_GetArrayItem(array,i);
                    copy_cJSON(receiverstr,cJSON_GetObjectItem(txobj,"recipient"));
                    if ( strcmp(receiverstr,INSTANTDEX_ACCT) == 0 )
                    {
                        if ( (senderbits = get_API_nxt64bits(cJSON_GetObjectItem(txobj,"sender"))) != 0 )
                        {
                            expand_nxt64bits(NXTaddr,senderbits);
                            np = get_NXTacct(&createdflag,Global_mp,NXTaddr);
                            amount = get_API_nxt64bits(cJSON_GetObjectItem(txobj,"amountNQT"));
                            if ( np->timestamp != now )
                            {
                                np->quantity = 0;
                                np->timestamp = now;
                            }
                            np->quantity += amount;
                            if ( maxnp == 0 || np->quantity > maxnp->quantity )
                                maxnp = np;
                        }
                    }
                }
            }
            free_json(json);
        }
        free(jsonstr);
    }
    if ( maxnp != 0 )
    {
        printf("Best MM %llu total %.8f\n",(long long)maxnp->H.nxt64bits,dstr(maxnp->quantity));
        return(maxnp->H.nxt64bits);
    }
    return(0);
}

int32_t calc_users_maxopentrades(uint64_t nxt64bits)
{
    return(13);
}

int32_t get_top_MMaker(struct pserver_info **pserverp)
{
    static uint64_t bestMMbits;
    struct nodestats *stats;
    char ipaddr[64];
    *pserverp = 0;
    if ( bestMMbits == 0 )
        bestMMbits = find_best_market_maker();
    if ( bestMMbits != 0 )
    {
        stats = get_nodestats(bestMMbits);
        expand_ipbits(ipaddr,stats->ipbits);
        (*pserverp) = get_pserver(0,ipaddr,0,0);
        return(0);
    }
    return(-1);
}

double calc_price_volume(double *volumep,uint64_t baseamount,uint64_t relamount)
{
    *volumep = ((double)baseamount / SATOSHIDEN);
    return((double)relamount / baseamount);
}

void set_best_amounts(uint64_t *baseamountp,uint64_t *relamountp,double price,double volume)
{
    double checkprice,checkvol,distA,distB,metric,bestmetric = (1. / SMALLVAL);
    uint64_t baseamount,relamount,bestbaseamount = 0,bestrelamount = 0;
    int32_t i,j;
    baseamount = volume * SATOSHIDEN;
    relamount = (price * baseamount);
    //*baseamountp = baseamount, *relamountp = relamount;
    //return;
    for (i=-1; i<=1; i++)
        for (j=-1; j<=1; j++)
        {
            checkprice = calc_price_volume(&checkvol,baseamount+i,relamount+j);
            distA = (checkprice - price);
            distA *= distA;
            distB = (checkvol - volume);
            distB *= distB;
            metric = sqrt(distA + distB);
            if ( metric < bestmetric )
            {
                bestmetric = metric;
                bestbaseamount = baseamount + i;
                bestrelamount = relamount + j;
                //printf("i.%d j.%d metric. %f\n",i,j,metric);
            }
        }
    *baseamountp = bestbaseamount;
    *relamountp = bestrelamount;
}

int32_t create_InstantDEX_quote(struct InstantDEX_quote *iQ,uint32_t timestamp,int32_t isask,uint64_t type,uint64_t nxt64bits,double price,double volume,uint64_t baseamount,uint64_t relamount)
{
    memset(iQ,0,sizeof(*iQ));
    if ( baseamount == 0 && relamount == 0 )
        set_best_amounts(&baseamount,&relamount,price,volume);
    iQ->timestamp = timestamp;
    iQ->type = type;
    iQ->isask = isask;
    iQ->nxt64bits = nxt64bits;
    iQ->baseamount = baseamount;
    iQ->relamount = relamount;
    return(0);
}

void add_user_order(struct rambook_info *rb,struct InstantDEX_quote *iQ)
{
    int32_t i;
    if ( rb->numquotes > 0 )
    {
        for (i=0; i<rb->numquotes; i++)
        {
            if ( memcmp(iQ,&rb->quotes[i],sizeof(rb->quotes[i])) == 0 )
                break;
        }
    } else i = 0;
    if ( i == rb->numquotes )
    {
        if ( i >= rb->maxquotes )
        {
            rb->maxquotes += 50;
            rb->quotes = realloc(rb->quotes,rb->maxquotes * sizeof(*rb->quotes));
            memset(&rb->quotes[i],0,50 * sizeof(*rb->quotes));
        }
        rb->quotes[rb->numquotes++] = *iQ;
    }
    //printf("add_user_order i.%d numquotes.%d\n",i,rb->numquotes);
}

void save_InstantDEX_quote(struct rambook_info *rb,struct InstantDEX_quote *iQ)
{
    char NXTaddr[64];
    struct NXT_acct *np;
    int32_t createdflag,maxallowed;
    maxallowed = calc_users_maxopentrades(iQ->nxt64bits);
    expand_nxt64bits(NXTaddr,iQ->nxt64bits);
    np = get_NXTacct(&createdflag,Global_mp,NXTaddr);
    if ( np->openorders >= maxallowed )
        purge_oldest_order(rb,iQ); // allow one pair per orderbook
    purge_oldest_order(rb,0);
    add_user_order(rb,iQ);
    np->openorders++;
}

struct rambook_info *_add_rambook_quote(struct rambook_info *rb,uint64_t nxt64bits,uint32_t timestamp,int32_t dir,double price,double volume,uint64_t baseamount,uint64_t relamount)
{
    struct InstantDEX_quote iQ;
    if ( timestamp == 0 )
        timestamp = (uint32_t)time(NULL);
    if ( dir > 0 )
        create_InstantDEX_quote(&iQ,timestamp,0,rb->assetids[3],nxt64bits,price,volume,baseamount,relamount);
    else
    {
        set_best_amounts(&baseamount,&relamount,price,volume);
        create_InstantDEX_quote(&iQ,timestamp,0,rb->assetids[3],nxt64bits,0,0,relamount,baseamount);
    }
    save_InstantDEX_quote(rb,&iQ);
    return(rb);
}

struct rambook_info *add_rambook_quote(char *exchange,struct InstantDEX_quote *iQ,uint64_t nxt64bits,uint32_t timestamp,int32_t dir,uint64_t baseid,uint64_t relid,double price,double volume,uint64_t baseamount,uint64_t relamount)
{
    struct rambook_info *rb;
    memset(iQ,0,sizeof(*iQ));
    if ( timestamp == 0 )
        timestamp = (uint32_t)time(NULL);
    if ( dir > 0 )
    {
        rb = get_rambook(baseid,relid,exchange);
        create_InstantDEX_quote(iQ,timestamp,0,rb->assetids[3],nxt64bits,price,volume,baseamount,relamount);
    }
    else
    {
        rb = get_rambook(relid,baseid,exchange);
        set_best_amounts(&baseamount,&relamount,price,volume);
        create_InstantDEX_quote(iQ,timestamp,0,rb->assetids[3],nxt64bits,0,0,relamount,baseamount);
    }
    save_InstantDEX_quote(rb,iQ);
    return(rb);
}

int32_t parseram_json_quotes(int32_t dir,struct rambook_info *rb,cJSON *array,int32_t maxdepth,char *pricefield,char *volfield)
{
    cJSON *item;
    int32_t i,n;
    uint32_t timestamp;
    double price,volume;
    timestamp = (uint32_t)time(NULL);
    n = cJSON_GetArraySize(array);
    if ( maxdepth != 0 && n > maxdepth )
        n = maxdepth;
    for (i=0; i<n; i++)
    {
        item = cJSON_GetArrayItem(array,i);
        if ( pricefield != 0 && volfield != 0 )
        {
            price = get_API_float(cJSON_GetObjectItem(item,pricefield));
            volume = get_API_float(cJSON_GetObjectItem(item,volfield));
        }
        else if ( is_cJSON_Array(item) != 0 && cJSON_GetArraySize(item) == 2 ) // big assumptions about order within nested array!
        {
            price = get_API_float(cJSON_GetArrayItem(item,0));
            volume = get_API_float(cJSON_GetArrayItem(item,1));
        }
        else
        {
            printf("unexpected case in parseram_json_quotes\n");
            continue;
        }
        if ( _add_rambook_quote(rb,stringbits(rb->exchange),timestamp,dir,price,volume,0,0) != rb )
            printf("ERROR: rambook mismatch for %s/%s dir.%d price %.8f vol %.8f\n",rb->base,rb->rel,dir,price,volume);
    }
    return(n);
}

void ramparse_json_orderbook(struct rambook_info *bids,struct rambook_info *asks,int32_t maxdepth,cJSON *json,char *resultfield,char *bidfield,char *askfield,char *pricefield,char *volfield)
{
    cJSON *obj = 0,*bidobj,*askobj;
    if ( resultfield == 0 )
        obj = json;
    if ( maxdepth == 0 )
        maxdepth = 10;
    if ( resultfield == 0 || (obj= cJSON_GetObjectItem(json,resultfield)) != 0 )
    {
        if ( (bidobj= cJSON_GetObjectItem(obj,bidfield)) != 0 && is_cJSON_Array(bidobj) != 0 )
            parseram_json_quotes(1,bids,bidobj,maxdepth,pricefield,volfield);
        if ( (askobj= cJSON_GetObjectItem(obj,askfield)) != 0 && is_cJSON_Array(askobj) != 0 )
            parseram_json_quotes(-1,asks,askobj,maxdepth,pricefield,volfield);
    }
}

void ramparse_cryptsy(struct rambook_info *bids,struct rambook_info *asks,int32_t maxdepth) // "BTC-BTCD"
{
    static char *marketstr = "[{\"42\":141},{\"AC\":199},{\"AGS\":253},{\"ALF\":57},{\"AMC\":43},{\"ANC\":66},{\"APEX\":257},{\"ARG\":48},{\"AUR\":160},{\"BC\":179},{\"BCX\":142},{\"BEN\":157},{\"BET\":129},{\"BLU\":251},{\"BQC\":10},{\"BTB\":23},{\"BTCD\":256},{\"BTE\":49},{\"BTG\":50},{\"BUK\":102},{\"CACH\":154},{\"CAIx\":221},{\"CAP\":53},{\"CASH\":150},{\"CAT\":136},{\"CGB\":70},{\"CINNI\":197},{\"CLOAK\":227},{\"CLR\":95},{\"CMC\":74},{\"CNC\":8},{\"CNL\":260},{\"COMM\":198},{\"COOL\":266},{\"CRC\":58},{\"CRYPT\":219},{\"CSC\":68},{\"DEM\":131},{\"DGB\":167},{\"DGC\":26},{\"DMD\":72},{\"DOGE\":132},{\"DRK\":155},{\"DVC\":40},{\"EAC\":139},{\"ELC\":12},{\"EMC2\":188},{\"EMD\":69},{\"EXE\":183},{\"EZC\":47},{\"FFC\":138},{\"FLT\":192},{\"FRAC\":259},{\"FRC\":39},{\"FRK\":33},{\"FST\":44},{\"FTC\":5},{\"GDC\":82},{\"GLC\":76},{\"GLD\":30},{\"GLX\":78},{\"GLYPH\":229},{\"GUE\":241},{\"HBN\":80},{\"HUC\":249},{\"HVC\":185},{\"ICB\":267},{\"IFC\":59},{\"IXC\":38},{\"JKC\":25},{\"JUDGE\":269},{\"KDC\":178},{\"KEY\":255},{\"KGC\":65},{\"LGD\":204},{\"LK7\":116},{\"LKY\":34},{\"LTB\":202},{\"LTC\":3},{\"LTCX\":233},{\"LYC\":177},{\"MAX\":152},{\"MEC\":45},{\"MIN\":258},{\"MINT\":156},{\"MN1\":187},{\"MN2\":196},{\"MNC\":7},{\"MRY\":189},{\"MYR\":200},{\"MZC\":164},{\"NAN\":64},{\"NAUT\":207},{\"NAV\":252},{\"NBL\":32},{\"NEC\":90},{\"NET\":134},{\"NMC\":29},{\"NOBL\":264},{\"NRB\":54},{\"NRS\":211},{\"NVC\":13},{\"NXT\":159},{\"NYAN\":184},{\"ORB\":75},{\"OSC\":144},{\"PHS\":86},{\"Points\":120},{\"POT\":173},{\"PPC\":28},{\"PSEUD\":268},{\"PTS\":119},{\"PXC\":31},{\"PYC\":92},{\"QRK\":71},{\"RDD\":169},{\"RPC\":143},{\"RT2\":235},{\"RYC\":9},{\"RZR\":237},{\"SAT2\":232},{\"SBC\":51},{\"SC\":225},{\"SFR\":270},{\"SHLD\":248},{\"SMC\":158},{\"SPA\":180},{\"SPT\":81},{\"SRC\":88},{\"STR\":83},{\"SUPER\":239},{\"SXC\":153},{\"SYNC\":271},{\"TAG\":117},{\"TAK\":166},{\"TEK\":114},{\"TES\":223},{\"TGC\":130},{\"TOR\":250},{\"TRC\":27},{\"UNB\":203},{\"UNO\":133},{\"URO\":247},{\"USDe\":201},{\"UTC\":163},{\"VIA\":261},{\"VOOT\":254},{\"VRC\":209},{\"VTC\":151},{\"WC\":195},{\"WDC\":14},{\"XC\":210},{\"XJO\":115},{\"XLB\":208},{\"XPM\":63},{\"XXX\":265},{\"YAC\":11},{\"YBC\":73},{\"ZCC\":140},{\"ZED\":170},{\"ZET\":85}]";
    
    int32_t i,m;
    cJSON *json,*obj,*marketjson;
    char *jsonstr,market[128];
    bids->numquotes = asks->numquotes = 0;
    if ( bids->url[0] == 0 )
    {
        if ( strcmp(bids->rel,"BTC") != 0 )
        {
            printf("parse_cryptsy: only BTC markets supported\n");
            return;
        }
        marketjson = cJSON_Parse(marketstr);
        if ( marketjson == 0 )
        {
            printf("parse_cryptsy: cant parse.(%s)\n",marketstr);
            return;
        }
        m = cJSON_GetArraySize(marketjson);
        market[0] = 0;
        for (i=0; i<m; i++)
        {
            obj = cJSON_GetArrayItem(marketjson,i);
            printf("(%s) ",get_cJSON_fieldname(obj));
            if ( strcmp(bids->base,get_cJSON_fieldname(obj)) == 0 )
            {
                printf("set market -> %llu\n",(long long)obj->child->valueint);
                sprintf(market,"%llu",(long long)obj->child->valueint);
                break;
            }
        }
        free(marketjson);
        if ( market[0] == 0 )
        {
            printf("parse_cryptsy: no marketid for %s\n",bids->base);
            return;
        }
        sprintf(bids->url,"http://pubapi.cryptsy.com/api.php?method=singleorderdata&marketid=%s",market);
    }
    bids->numquotes = asks->numquotes = 0;
    jsonstr = _issue_curl(0,"cryptsy",bids->url);
    if ( jsonstr != 0 )
    {
        if ( (json = cJSON_Parse(jsonstr)) != 0 )
        {
            if ( get_cJSON_int(json,"success") == 1 && (obj= cJSON_GetObjectItem(json,"return")) != 0 )
                ramparse_json_orderbook(bids,asks,maxdepth,obj,bids->base,"buyorders","sellorders","price","quantity");
            free_json(json);
        }
        free(jsonstr);
    }
}

void ramparse_bittrex(struct rambook_info *bids,struct rambook_info *asks,int32_t maxdepth) // "BTC-BTCD"
{
    cJSON *json,*obj;
    char *jsonstr,market[128];
    bids->numquotes = asks->numquotes = 0;
    if ( bids->url[0] == 0 )
    {
        sprintf(market,"%s-%s",bids->rel,bids->base);
        sprintf(bids->url,"https://bittrex.com/api/v1.1/public/getorderbook?market=%s&type=both&depth=%d",market,maxdepth);
    }
    jsonstr = _issue_curl(0,"trex",bids->url);
    //printf("(%s) -> (%s)\n",ep->url,jsonstr);
    // {"success":true,"message":"","result":{"buy":[{"Quantity":1.69284935,"Rate":0.00122124},{"Quantity":130.39771416,"Rate":0.00122002},{"Quantity":77.31781088,"Rate":0.00122000},{"Quantity":10.00000000,"Rate":0.00120150},{"Quantity":412.23470195,"Rate":0.00119500}],"sell":[{"Quantity":8.58353086,"Rate":0.00123019},{"Quantity":10.93796714,"Rate":0.00127744},{"Quantity":17.96825904,"Rate":0.00128000},{"Quantity":2.80400381,"Rate":0.00129999},{"Quantity":200.00000000,"Rate":0.00130000}]}}
    if ( jsonstr != 0 )
    {
        if ( (json = cJSON_Parse(jsonstr)) != 0 )
        {
            if ( (obj= cJSON_GetObjectItem(json,"success")) != 0 && is_cJSON_True(obj) != 0 )
                ramparse_json_orderbook(bids,asks,maxdepth,json,"result","buy","sell","Rate","Quantity");
            free_json(json);
        }
        free(jsonstr);
    }
}

void ramparse_poloniex(struct rambook_info *bids,struct rambook_info *asks,int32_t maxdepth)
{
    cJSON *json;
    char *jsonstr,market[128];
    bids->numquotes = asks->numquotes = 0;
    if ( bids->url[0] == 0 )
    {
        sprintf(market,"%s_%s",bids->rel,bids->base);
        sprintf(bids->url,"https://poloniex.com/public?command=returnOrderBook&currencyPair=%s&depth=%d",market,maxdepth);
    }
    jsonstr = _issue_curl(0,"polo",bids->url);
    //{"asks":[[7.975e-5,200],[7.98e-5,108.46834002],[7.998e-5,1.2644054],[8.0e-5,1799],[8.376e-5,1.7528442],[8.498e-5,30.25055195],[8.499e-5,570.01953171],[8.5e-5,1399.91458777],[8.519e-5,123.82790941],[8.6e-5,1000],[8.696e-5,1.3914002],[8.7e-5,1000],[8.723e-5,112.26190114],[8.9e-5,181.28118327],[8.996e-5,1.237759],[9.0e-5,270.096],[9.049e-5,2993.99999999],[9.05e-5,3383.48549983],[9.068e-5,2.52582092],[9.1e-5,30],[9.2e-5,40],[9.296e-5,1.177861],[9.3e-5,81.59999998],[9.431e-5,2],[9.58e-5,1.074289],[9.583e-5,3],[9.644e-5,858.48948115],[9.652e-5,3441.55358115],[9.66e-5,15699.9569377],[9.693e-5,23.5665998],[9.879e-5,1.0843656],[9.881e-5,2],[9.9e-5,700],[9.999e-5,123.752],[0.0001,34.04293],[0.00010397,1.742916],[0.00010399,11.24446],[0.00010499,1497.79999999],[0.00010799,1.2782902],[0.000108,1818.80661458],[0.00011,1395.27245417],[0.00011407,0.89460453],[0.00011409,0.89683778],[0.0001141,0.906],[0.00011545,458.09939081],[0.00011599,5],[0.00011798,1.0751625],[0.00011799,5],[0.00011999,5.86],[0.00012,279.64865088]],"bids":[[7.415e-5,4495],[7.393e-5,1.8650999],[7.392e-5,974.53828463],[7.382e-5,896.34272554],[7.381e-5,3000],[7.327e-5,1276.26600246],[7.326e-5,77.32705432],[7.32e-5,190.98472093],[7.001e-5,2.2642602],[7.0e-5,1112.19485714],[6.991e-5,2000],[6.99e-5,5000],[6.951e-5,500],[6.914e-5,91.63013181],[6.9e-5,500],[6.855e-5,500],[6.85e-5,238.86947265],[6.212e-5,5.2800413],[6.211e-5,4254.38737723],[6.0e-5,1697.3335],[5.802e-5,3.1241932],[5.801e-5,4309.60179279],[5.165e-5,20],[5.101e-5,6.2903434],[5.1e-5,100],[5.0e-5,5000],[4.5e-5,15],[3.804e-5,16.67907],[3.803e-5,30],[3.002e-5,1400],[3.001e-5,15.320937],[3.0e-5,10000],[2.003e-5,32.345771],[2.002e-5,50],[2.0e-5,25000],[1.013e-5,79.250137],[1.012e-5,200],[1.01e-5,200000],[2.0e-7,5000],[1.9e-7,5000],[1.4e-7,1644.2107],[1.2e-7,1621.8622],[1.1e-7,10000],[1.0e-7,100000],[6.0e-8,4253.7528],[4.0e-8,3690.3146],[3.0e-8,100000],[1.0e-8,100000]],"isFrozen":"0"}
    if ( jsonstr != 0 )
    {
        if ( (json = cJSON_Parse(jsonstr)) != 0 )
        {
            ramparse_json_orderbook(bids,asks,maxdepth,json,0,"bids","asks",0,0);
            free_json(json);
        }
        free(jsonstr);
    }
}

void ramparse_bitfinex(struct rambook_info *bids,struct rambook_info *asks,int32_t maxdepth)
{
    cJSON *json;
    char *jsonstr;
    bids->numquotes = asks->numquotes = 0;
    if ( bids->url[0] == 0 )
        sprintf(bids->url,"https://api.bitfinex.com/v1/book/%s%s",bids->base,bids->rel);
    jsonstr = _issue_curl(0,"bitfinex",bids->url);
    //{"bids":[{"price":"239.78","amount":"12.0","timestamp":"1424748729.0"},{"p
    if ( jsonstr != 0 )
    {
        if ( (json = cJSON_Parse(jsonstr)) != 0 )
        {
            ramparse_json_orderbook(bids,asks,maxdepth,json,0,"bids","asks","price","amount");
            free_json(json);
        }
        free(jsonstr);
    }
}

cJSON *convram_NXT_quotejson(cJSON *json,char *fieldname,uint64_t ap_mult)
{
    //"priceNQT": "12900",
    //"asset": "4551058913252105307",
    //"order": "8128728940342496249",
    //"quantityQNT": "20000000",
    int32_t i,n;
    double price,vol,factor;
    cJSON *srcobj,*srcitem,*inner,*array = 0;
    if ( ap_mult == 0 )
        return(0);
    factor = (double)ap_mult / SATOSHIDEN;
    srcobj = cJSON_GetObjectItem(json,fieldname);
    if ( srcobj != 0 )
    {
        if ( (n= cJSON_GetArraySize(srcobj)) > 0 )
        {
            array = cJSON_CreateArray();
            for (i=0; i<n; i++)
            {
                srcitem = cJSON_GetArrayItem(srcobj,i);
                price = (double)get_satoshi_obj(srcitem,"priceNQT") / ap_mult;
                vol = (double)get_satoshi_obj(srcitem,"quantityQNT") * factor;
                inner = cJSON_CreateArray();
                cJSON_AddItemToArray(inner,cJSON_CreateNumber(price));
                cJSON_AddItemToArray(inner,cJSON_CreateNumber(vol));
                cJSON_AddItemToArray(array,inner);
            }
        }
        free_json(json);
        json = array;
    }
    else free_json(json), json = 0;
    return(json);
}

void ramparse_NXT(struct rambook_info *bids,struct rambook_info *asks,int32_t maxdepth)
{
    cJSON *json,*bidobj,*askobj;
    char *buystr,*sellstr;
    struct NXT_asset *ap;
    int32_t createdflag;
    char assetidstr[64];
    if ( NXT_ASSETID != stringbits("NXT") )
        printf("NXT_ASSETID.%llu != %llu stringbits\n",(long long)NXT_ASSETID,(long long)stringbits("NXT"));
    bids->numquotes = asks->numquotes = 0;
    expand_nxt64bits(assetidstr,bids->assetids[0]);
    ap = get_NXTasset(&createdflag,Global_mp,assetidstr);
    if ( bids->assetids[1] != NXT_ASSETID && bids->assetids[0] != NXT_ASSETID )
    {
        printf("NXT only supports trading against NXT not %llu %llu\n",(long long)bids->assetids[0],(long long)bids->assetids[1]);
        return;
    }
    if ( bids->url[0] == 0 )
        sprintf(bids->url,"%s=getBidOrders&asset=%llu&limit=%d",NXTSERVER,(long long)bids->assetids[0],maxdepth);
    if ( asks->url[0] == 0 )
        sprintf(asks->url,"%s=getAskOrders&asset=%llu&limit=%d",NXTSERVER,(long long)asks->assetids[1],maxdepth);
    buystr = _issue_curl(0,"ramparse",bids->url);
    sellstr = _issue_curl(0,"ramparse",asks->url);
    //{"count":3,"type":"BUY","orders":[{"price":"0.00000003","amount":"137066327.96066495","total":"4.11198982"},{"price":"0.00000002","amount":"293181381.39291047","total":"5.86362760"},{"price":"0.00000001","amount":"493836943.18472463","total":"4.93836939"}]}
    if ( buystr != 0 && sellstr != 0 )
    {
        bidobj = askobj = 0;
        if ( (json = cJSON_Parse(buystr)) != 0 )
            bidobj = convram_NXT_quotejson(json,"bidOrders",bids->basemult);
        if ( (json = cJSON_Parse(sellstr)) != 0 )
            askobj = convram_NXT_quotejson(json,"askOrders",asks->basemult);
        json = cJSON_CreateObject();
        if ( bidobj != 0 )
            cJSON_AddItemToObject(json,"bids",bidobj);
        if ( askobj != 0 )
            cJSON_AddItemToObject(json,"asks",askobj);
        ramparse_json_orderbook(bids,asks,maxdepth,json,0,"bids","asks",0,0);
        free_json(json);
    }
    if ( buystr != 0 )
        free(buystr);
    if ( sellstr != 0 )
        free(sellstr);
}

int32_t init_rambooks(char *base,char *rel,uint64_t baseid,uint64_t relid)
{
    int32_t i,n = 0;
    printf("init_rambooks.(%s %s)\n",base,rel);
    if ( base != 0 && rel != 0 && base[0] != 0 && rel[0] != 0 )
    {
        baseid = stringbits(base);
        relid = stringbits(rel);
        if ( strcmp(base,"BTC") == 0 || strcmp(rel,"BTC") == 0 )
        {
            get_rambook(baseid,relid,"cryptsy"), get_rambook(relid,baseid,"cryptsy");
            get_rambook(baseid,relid,"bittrex"), get_rambook(relid,baseid,"bittrex");
            get_rambook(baseid,relid,"poloniex"), get_rambook(relid,baseid,"poloniex");
            get_rambook(baseid,relid,"bitfinex"), get_rambook(relid,baseid,"bitfinex");
        }
        if ( strcmp(base,"NXT") == 0 || strcmp(rel,"NXT") == 0 )
        {
            if ( strcmp(base,"NXT") != 0 )
            {
                for (i=0; i<(int32_t)(sizeof(assetmap)/sizeof(*assetmap)); i++)
                    if ( strcmp(assetmap[i][1],base) == 0 )
                    {
                        baseid = calc_nxt64bits(assetmap[i][0]);
                        get_rambook(baseid,relid,"nxtae");
                        get_rambook(relid,baseid,"nxtae");
                    }
            }
            else
            {
                for (i=0; i<(int32_t)(sizeof(assetmap)/sizeof(*assetmap)); i++)
                    if ( strcmp(assetmap[i][1],base) == 0 )
                    {
                        relid = calc_nxt64bits(assetmap[i][0]);
                        get_rambook(baseid,relid,"nxtae");
                        get_rambook(relid,baseid,"nxtae");
                    }
            }
        }
    }
    else // NXT only
    {
        get_rambook(baseid,relid,"nxtae");
        get_rambook(relid,baseid,"nxtae");
    }
    return(n);
}

cJSON *gen_orderbook_item(struct InstantDEX_quote *iQ,int32_t allflag,uint64_t baseid,uint64_t relid)
{
    char offerstr[MAX_JSON_FIELD];
    double price,volume;
    uint64_t baseamount,relamount;
    if ( iQ->baseamount != 0 && iQ->relamount != 0 )
    {
        if ( iQ->isask != 0 )
            baseamount = iQ->relamount, relamount = iQ->baseamount;
        else baseamount = iQ->baseamount, relamount = iQ->relamount;
        price = calc_price_volume(&volume,baseamount,relamount);
        if ( allflag != 0 )
        {
            sprintf(offerstr,"{\"price\":\"%.11f\",\"volume\":\"%.8f\",\"requestType\":\"makeoffer\",\"baseid\":\"%llu\",\"baseamount\":\"%llu\",\"realid\":\"%llu\",\"relamount\":\"%llu\",\"other\":\"%llu\",\"type\":\"%llu\"}",price,volume,(long long)baseid,(long long)baseamount,(long long)relid,(long long)relamount,(long long)iQ->nxt64bits,(long long)iQ->type);
        }
        else sprintf(offerstr,"{\"price\":\"%.11f\",\"volume\":\"%.8f\"}",price,volume);
    }
    return(cJSON_Parse(offerstr));
}

cJSON *gen_InstantDEX_json(struct rambook_info *rb,int32_t isask,struct InstantDEX_quote *iQ,uint64_t refbaseid,uint64_t refrelid)
{
    cJSON *json = cJSON_CreateObject();
    char numstr[64],base[64],rel[64];
    double price,volume;
    uint64_t mult;
    price = calc_price_volume(&volume,iQ->baseamount,iQ->relamount);
    cJSON_AddItemToObject(json,"requestType",cJSON_CreateString((isask != 0) ? "ask" : "bid"));
    set_assetname(&mult,base,refbaseid), cJSON_AddItemToObject(json,"base",cJSON_CreateString(base));
    set_assetname(&mult,rel,refrelid), cJSON_AddItemToObject(json,"rel",cJSON_CreateString(rel));
    cJSON_AddItemToObject(json,"price",cJSON_CreateNumber(price));
    cJSON_AddItemToObject(json,"volume",cJSON_CreateNumber(volume));
    
    cJSON_AddItemToObject(json,"timestamp",cJSON_CreateNumber(iQ->timestamp));
    cJSON_AddItemToObject(json,"age",cJSON_CreateNumber((uint32_t)time(NULL) - iQ->timestamp));
    sprintf(numstr,"%llu",(long long)iQ->type), cJSON_AddItemToObject(json,"type",cJSON_CreateString(numstr));
    sprintf(numstr,"%llu",(long long)iQ->nxt64bits), cJSON_AddItemToObject(json,"NXT",cJSON_CreateString(numstr));
    
    sprintf(numstr,"%llu",(long long)refbaseid), cJSON_AddItemToObject(json,"baseid",cJSON_CreateString(numstr));
    sprintf(numstr,"%llu",(long long)iQ->baseamount), cJSON_AddItemToObject(json,"baseamount",cJSON_CreateString(numstr));
    sprintf(numstr,"%llu",(long long)refrelid), cJSON_AddItemToObject(json,"relid",cJSON_CreateString(numstr));
    sprintf(numstr,"%llu",(long long)iQ->relamount), cJSON_AddItemToObject(json,"relamount",cJSON_CreateString(numstr));
    return(json);
}

int _decreasing_quotes(const void *a,const void *b)
{
#define iQ_a ((struct InstantDEX_quote *)a)
#define iQ_b ((struct InstantDEX_quote *)b)
    double vala,valb,volume;
    vala = calc_price_volume(&volume,iQ_a->baseamount,iQ_a->relamount);
    valb = calc_price_volume(&volume,iQ_b->baseamount,iQ_b->relamount);
	if ( valb > vala )
		return(1);
	else if ( valb < vala )
		return(-1);
	return(0);
#undef iQ_a
#undef iQ_b
}

int _increasing_quotes(const void *a,const void *b)
{
#define iQ_a ((struct InstantDEX_quote *)a)
#define iQ_b ((struct InstantDEX_quote *)b)
    double vala,valb,volume;
    vala = calc_price_volume(&volume,iQ_a->baseamount,iQ_a->relamount);
    valb = calc_price_volume(&volume,iQ_b->baseamount,iQ_b->relamount);
	if ( valb > vala )
		return(-1);
	else if ( valb < vala )
		return(1);
	return(0);
#undef iQ_a
#undef iQ_b
}

char *allorderbooks_func(char *NXTaddr,char *NXTACCTSECRET,char *previpaddr,char *sender,int32_t valid,cJSON **objs,int32_t numobjs,char *origargstr)
{
    cJSON *json;
    char *jsonstr;
    if ( (json= all_orderbooks()) != 0 )
    {
        jsonstr = cJSON_Print(json);
        free_json(json);
        return(jsonstr);
    }
    return(clonestr("{\"error\":\"no orderbooks\"}"));
}

char *openorders_func(char *NXTaddr,char *NXTACCTSECRET,char *previpaddr,char *sender,int32_t valid,cJSON **objs,int32_t numobjs,char *origargstr)
{
    cJSON *array,*json = 0;
    struct rambook_info **obooks,*rb;
    struct InstantDEX_quote *iQ;
    int32_t i,j,numbooks,n = 0;
    char nxtaddr[64],*jsonstr;
    if ( (obooks= get_allrambooks(&numbooks)) != 0 )
    {
        array = cJSON_CreateArray();
        for (i=0; i<numbooks; i++)
        {
            rb = obooks[i];
            if ( rb->numquotes == 0 )
                continue;
            for (j=0; j<rb->numquotes; j++)
            {
                iQ = &rb->quotes[j];
                expand_nxt64bits(nxtaddr,iQ->nxt64bits);
                if ( strcmp(NXTaddr,nxtaddr) == 0 )
                    cJSON_AddItemToArray(array,gen_InstantDEX_json(rb,iQ->isask,iQ,rb->assetids[0],rb->assetids[1])), n++;
            }
        }
        free(obooks);
        if ( n > 0 )
        {
            json = cJSON_CreateObject();
            cJSON_AddItemToObject(json,"openorders",array);
            jsonstr = cJSON_Print(json);
            free_json(json);
            return(jsonstr);
        }
    }
    return(clonestr("{\"result\":\"no openorders\"}"));
}

uint64_t is_feetx_unconfirmed(uint64_t feetxid,uint64_t fee,char *fullhash)
{
    uint64_t amount,senderbits = 0;
    char cmd[1024],sender[MAX_JSON_FIELD],txidstr[MAX_JSON_FIELD],reftx[MAX_JSON_FIELD],*jsonstr;
    cJSON *json,*array,*txobj;
    int32_t i,n,type,subtype;
    sprintf(cmd,"requestType=getUnconfirmedTransactions&account=%s",INSTANTDEX_ACCT);
    if ( (jsonstr= issue_NXTPOST(0,cmd)) != 0 )
    {
        printf("getUnconfirmedTransactions (%s)\n",jsonstr);
        if ( (json= cJSON_Parse(jsonstr)) != 0 )
        {
            if ( (array= cJSON_GetObjectItem(json,"unconfirmedTransactions")) != 0 && is_cJSON_Array(array) != 0 && (n= cJSON_GetArraySize(array)) > 0 )
            {
                for (i=0; i<n; i++)
                {
                    txobj = cJSON_GetArrayItem(array,i);
                    copy_cJSON(txidstr,cJSON_GetObjectItem(txobj,"transaction"));
                    if ( calc_nxt64bits(txidstr) == feetxid )
                    {
                        copy_cJSON(reftx,cJSON_GetObjectItem(txobj,"referencedTransactionFullHash"));
                        copy_cJSON(sender,cJSON_GetObjectItem(txobj,"sender"));
                        type = (int32_t)get_API_int(cJSON_GetObjectItem(txobj,"type"),-1);
                        subtype = (int32_t)get_API_int(cJSON_GetObjectItem(txobj,"subtype"),-1);
                        amount = get_API_nxt64bits(cJSON_GetObjectItem(txobj,"amountNQT"));
                        printf("found unconfirmed feetxid from %s for %.8f\n",sender,dstr(amount));
                        if ( type == 0 && subtype == 0 && amount >= fee && (fullhash == 0 || strcmp(fullhash,reftx) == 0) )
                            senderbits = calc_nxt64bits(sender);
                        break;
                    }
                }
            }
            free_json(json);
        }
        free(jsonstr);
    }
    return(senderbits);
}

struct InstantDEX_quote *order_match(uint64_t nxt64bits,uint64_t relid,uint64_t relqty,uint64_t othernxtbits,uint64_t baseid,uint64_t baseqty,uint64_t relfee,uint64_t relfeetxid,char *fullhash)
{
    //struct NXT_asset *ap;
    char otherNXTaddr[64],assetidstr[64];
    struct InstantDEX_quote *iQ;
    struct rambook_info **obooks,*rb;
    int32_t i,j,numbooks;
    uint64_t baseamount,relamount,otherbalance;
    int64_t unconfirmed;
    expand_nxt64bits(otherNXTaddr,othernxtbits);
    expand_nxt64bits(assetidstr,baseid);
    if ( (obooks= get_allrambooks(&numbooks)) != 0 )
    {
        for (i=0; i<numbooks; i++)
        {
            rb = obooks[i];
            baseamount = (baseqty * rb->basemult);
            relamount = ((relqty + 0*relfee) * rb->relmult);
            printf("[%llu %llu] checking base.%llu %llu %.8f -> %llu %.8f rel.%llu | rb %llu -> %llu\n",(long long)rb->basemult,(long long)rb->relmult,(long long)baseid,(long long)baseqty,dstr(baseamount),(long long)relqty,dstr(relamount),(long long)relid,(long long)rb->assetids[0],(long long)rb->assetids[1]);
            if ( rb->numquotes == 0 || rb->assetids[0] != baseid || rb->assetids[1] != relid )
                continue;
            for (j=0; j<rb->numquotes; j++)
            {
                iQ = &rb->quotes[j];
                printf(" %llu >= %llu and %llu <= %llu relfee.%llu\n",(long long)baseamount,(long long)iQ->baseamount,(long long)relamount,(long long)iQ->relamount,(long long)relfee);
                if ( iQ->matched == 0 && iQ->nxt64bits == nxt64bits && baseamount >= iQ->baseamount && relamount <= iQ->relamount )
                {
                    otherbalance = get_asset_quantity(&unconfirmed,otherNXTaddr,assetidstr);
                    printf("MATCHED! %llu >= %llu and %llu <= %llu relfee.%llu otherbalance %.8f unconfirmed %.8f\n",(long long)baseamount,(long long)iQ->baseamount,(long long)relamount,(long long)iQ->relamount,(long long)relfee,dstr(otherbalance),dstr(unconfirmed));
                    if ( is_feetx_unconfirmed(relfeetxid,relfee,fullhash) == othernxtbits && otherbalance >= (baseqty - 0*relfee) )
                    {
                        printf("MATCHED for real\n");
                        iQ->matched = 1;
                        free(obooks);
                        return(iQ);
                    }
                }
            }
        }
        free(obooks);
    }
    return(0);
}

void free_orderbook(struct orderbook *op)
{
    if ( op != 0 )
    {
        if ( op->bids != 0 )
            free(op->bids);
        if ( op->asks != 0 )
            free(op->asks);
        free(op);
    }
}

void update_orderbook(int32_t iter,struct orderbook *op,int32_t *numbidsp,int32_t *numasksp,struct InstantDEX_quote *iQ,int32_t polarity)
{
    struct InstantDEX_quote *ask;
    if ( iter == 0 )
    {
        if ( polarity > 0 )
            op->numbids++;
        else op->numasks++;
    }
    else
    {
        if ( polarity > 0 )
            op->bids[(*numbidsp)++] = *iQ;
        else
        {
            ask = &op->asks[(*numasksp)++];
            *ask = *iQ;
            ask->baseamount = iQ->relamount;
            ask->relamount = iQ->baseamount;
        }
    }
}

void add_to_orderbook(struct orderbook *op,int32_t iter,int32_t *numbidsp,int32_t *numasksp,struct rambook_info *rb,struct InstantDEX_quote *iQ,int32_t polarity,int32_t oldest)
{
    if ( iQ->timestamp >= oldest )
        update_orderbook(iter,op,numbidsp,numasksp,iQ,polarity);
}

struct orderbook *create_orderbook(uint32_t oldest,uint64_t refbaseid,uint64_t refrelid)
{
    int32_t i,j,iter,numbids,numasks,numbooks,polarity,haveexchanges = 0;
    char obookstr[64];
    struct rambook_info **obooks,*rb;
    struct orderbook *op = 0;
    uint64_t basemult,relmult;
    expand_nxt64bits(obookstr,_obookid(refbaseid,refrelid));
    op = (struct orderbook *)calloc(1,sizeof(*op));
    set_assetname(&basemult,op->base,refbaseid);
    set_assetname(&relmult,op->rel,refrelid);
    op->baseid = refbaseid;
    op->relid = refrelid;
    for (iter=0; iter<2; iter++)
    {
        numbids = numasks = 0;
        if ( (obooks= get_allrambooks(&numbooks)) != 0 )
        {
            printf("got %d rambooks\n",numbooks);
            for (i=0; i<numbooks; i++)
            {
                rb = obooks[i];
                if ( strcmp(rb->exchange,INSTANTDEX_NAME) != 0 )
                    haveexchanges++;
                if ( rb->numquotes == 0 )
                    continue;
                if ( rb->assetids[0] == refbaseid && rb->assetids[1] == refrelid )
                    polarity = 1;
                else if ( rb->assetids[1] == refbaseid && rb->assetids[0] == refrelid )
                    polarity = -1;
                else continue;
                for (j=0; j<rb->numquotes; j++)
                    add_to_orderbook(op,iter,&numbids,&numasks,rb,&rb->quotes[j],polarity,oldest);
            }
            free(obooks);
        }
        if ( iter == 0 )
        {
            if ( op->numbids > 0 )
                op->bids = (struct InstantDEX_quote *)calloc(op->numbids,sizeof(*op->bids));
            if ( op->numasks > 0 )
                op->asks = (struct InstantDEX_quote *)calloc(op->numasks,sizeof(*op->asks));
        }
        else
        {
            if ( op->numbids > 0 || op->numasks > 0 )
            {
                if ( op->numbids > 0 )
                    qsort(op->bids,op->numbids,sizeof(*op->bids),_decreasing_quotes);
                if ( op->numasks > 0 )
                    qsort(op->asks,op->numasks,sizeof(*op->asks),_increasing_quotes);
            }
        }
    }
    //printf("(%f %f %llu %u)\n",quotes->price,quotes->vol,(long long)quotes->nxt64bits,quotes->timestamp);
    if ( op != 0 )
    {
        if ( haveexchanges == 0 )
            init_rambooks(op->base,op->rel,refbaseid,refrelid);
        free(op);
        op = 0;
    }
    return(op);
}

char *orderbook_func(char *NXTaddr,char *NXTACCTSECRET,char *previpaddr,char *sender,int32_t valid,cJSON **objs,int32_t numobjs,char *origargstr)
{
    uint32_t oldest;
    int32_t i,allflag,maxdepth;
    uint64_t baseid,relid;
    cJSON *json,*bids,*asks,*item;
    struct orderbook *op;
    char obook[64],buf[MAX_JSON_FIELD],baserel[128],datastr[MAX_JSON_FIELD],assetA[64],assetB[64],*retstr = 0;
    baseid = get_API_nxt64bits(objs[0]);
    relid = get_API_nxt64bits(objs[1]);
    allflag = get_API_int(objs[2],0);
    oldest = get_API_int(objs[3],0);
    maxdepth = get_API_int(objs[4],10);
    expand_nxt64bits(obook,_obookid(baseid,relid));
    sprintf(buf,"{\"baseid\":\"%llu\",\"relid\":\"%llu\",\"oldest\":%u}",(long long)baseid,(long long)relid,oldest);
    init_hexbytes_noT(datastr,(uint8_t *)buf,strlen(buf));
    //printf("ORDERBOOK.(%s)\n",buf);
    retstr = 0;
    if ( baseid != 0 && relid != 0 && (op= create_orderbook(oldest,baseid,relid)) != 0 )
    {
        if ( op->numbids == 0 && op->numasks == 0 )
            retstr = clonestr("{\"error\":\"no bids or asks\"}");
        else
        {
            json = cJSON_CreateObject();
            bids = cJSON_CreateArray();
            for (i=0; i<op->numbids; i++)
            {
                if ( (item= gen_orderbook_item(&op->bids[i],allflag,baseid,relid)) != 0 )
                    cJSON_AddItemToArray(bids,item);
            }
            asks = cJSON_CreateArray();
            for (i=0; i<op->numasks; i++)
            {
                if ( (item= gen_orderbook_item(&op->asks[i],allflag,baseid,relid)) != 0 )
                    cJSON_AddItemToArray(asks,item);
            }
            expand_nxt64bits(assetA,op->baseid);
            expand_nxt64bits(assetB,op->relid);
            sprintf(baserel,"%s/%s",op->base,op->rel);
            cJSON_AddItemToObject(json,"pair",cJSON_CreateString(baserel));
            cJSON_AddItemToObject(json,"obookid",cJSON_CreateString(obook));
            cJSON_AddItemToObject(json,"baseid",cJSON_CreateString(assetA));
            cJSON_AddItemToObject(json,"relid",cJSON_CreateString(assetB));
            cJSON_AddItemToObject(json,"bids",bids);
            cJSON_AddItemToObject(json,"asks",asks);
            cJSON_AddItemToObject(json,"NXT",cJSON_CreateString(NXTaddr));
            retstr = cJSON_Print(json);
        }
        free_orderbook(op);
    }
    else
    {
        sprintf(buf,"{\"error\":\"no such orderbook.(%llu ^ %llu)\"}",(long long)baseid,(long long)relid);
        retstr = clonestr(buf);
    }
    return(retstr);
}

void submit_quote(char *quotestr)
{
    int32_t len;
    char _tokbuf[4096];
    struct pserver_info *pserver;
    struct coin_info *cp = get_coin_info("BTCD");
    if ( cp != 0 )
    {
        printf("submit_quote.(%s)\n",quotestr);
        len = construct_tokenized_req(_tokbuf,quotestr,cp->srvNXTACCTSECRET);
        if ( get_top_MMaker(&pserver) == 0 )
            call_SuperNET_broadcast(pserver,_tokbuf,len,ORDERBOOK_EXPIRATION);
        call_SuperNET_broadcast(0,_tokbuf,len,ORDERBOOK_EXPIRATION);
    }
}

char *placequote_func(char *previpaddr,int32_t dir,char *sender,int32_t valid,cJSON **objs,int32_t numobjs,char *origargstr)
{
    cJSON *json;
    uint64_t type,baseamount,relamount,nxt64bits,baseid,relid,txid = 0;
    double price,volume;
    uint32_t timestamp;
    int32_t remoteflag;
    struct rambook_info *rb;
    struct InstantDEX_quote iQ;
    char buf[MAX_JSON_FIELD],txidstr[64],*jsonstr,*retstr = 0;
    remoteflag = (is_remote_access(previpaddr) != 0);
    nxt64bits = calc_nxt64bits(sender);
    baseid = get_API_nxt64bits(objs[0]);
    relid = get_API_nxt64bits(objs[1]);
    if ( baseid == 0 || relid == 0 )
        return(clonestr("{\"error\":\"illegal asset id\"}"));
    baseamount = get_API_nxt64bits(objs[5]);
    relamount = get_API_nxt64bits(objs[6]);
    if ( baseamount != 0 && relamount != 0 )
        price = calc_price_volume(&volume,baseamount,relamount);
    else
    {
        volume = get_API_float(objs[2]);
        price = get_API_float(objs[3]);
    }
    type = get_API_nxt64bits(objs[7]);
    timestamp = (uint32_t)get_API_int(objs[4],0);
    printf("t.%u placequote type.%llu dir.%d sender.(%s) valid.%d price %.11f vol %.8f\n",timestamp,(long long)type,dir,sender,valid,price,volume);
    if ( sender[0] != 0 && valid > 0 )
    {
        if ( price != 0. && volume != 0. && dir != 0 )
        {
            rb = add_rambook_quote(INSTANTDEX_NAME,&iQ,nxt64bits,timestamp,dir,baseid,relid,price,volume,baseamount,relamount);
            if ( remoteflag == 0 && (json= gen_InstantDEX_json(rb,0,&iQ,rb->assetids[0],rb->assetids[1])) != 0 )
            {
                jsonstr = cJSON_Print(json);
                stripwhite_ns(jsonstr,strlen(jsonstr));
                submit_quote(jsonstr);
                free_json(json);
                free(jsonstr);
            }
            txid = calc_txid((uint8_t *)&iQ,sizeof(iQ));
            if ( txid != 0 )
            {
                expand_nxt64bits(txidstr,txid);
                sprintf(buf,"{\"result\":\"success\",\"txid\":\"%s\"}",txidstr);
                retstr = clonestr(buf);
                printf("placequote.(%s)\n",buf);
            }
        }
        if ( retstr == 0 )
        {
            sprintf(buf,"{\"error submitting\":\"place%s error %llu/%llu volume %f price %f\"}",dir>0?"bid":"ask",(long long)baseid,(long long)relid,volume,price);
            retstr = clonestr(buf);
        }
    }
    else
    {
        sprintf(buf,"{\"error\":\"place%s error %llu/%llu dir.%d volume %f price %f\"}",dir>0?"bid":"ask",(long long)baseid,(long long)relid,dir,volume,price);
        retstr = clonestr(buf);
    }
    return(retstr);
}

char *placebid_func(char *NXTaddr,char *NXTACCTSECRET,char *previpaddr,char *sender,int32_t valid,cJSON **objs,int32_t numobjs,char *origargstr)
{
    return(placequote_func(previpaddr,1,sender,valid,objs,numobjs,origargstr));
}

char *placeask_func(char *NXTaddr,char *NXTACCTSECRET,char *previpaddr,char *sender,int32_t valid,cJSON **objs,int32_t numobjs,char *origargstr)
{
    return(placequote_func(previpaddr,-1,sender,valid,objs,numobjs,origargstr));
}

char *bid_func(char *NXTaddr,char *NXTACCTSECRET,char *previpaddr,char *sender,int32_t valid,cJSON **objs,int32_t numobjs,char *origargstr)
{
    return(placequote_func(previpaddr,1,sender,valid,objs,numobjs,origargstr));
}

char *ask_func(char *NXTaddr,char *NXTACCTSECRET,char *previpaddr,char *sender,int32_t valid,cJSON **objs,int32_t numobjs,char *origargstr)
{
    return(placequote_func(previpaddr,-1,sender,valid,objs,numobjs,origargstr));
}


#undef _obookid


#endif
