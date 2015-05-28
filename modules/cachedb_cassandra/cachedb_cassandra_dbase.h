/*
 * Copyright (C) 2011 OpenSIPS Solutions
 *
 * This file is part of opensips, a free SIP server.
 *
 * opensips is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * opensips is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 *
 * history:
 * ---------
 *  2011-12-xx  created (vlad-paiu)
 */

#ifndef CACHEDBCASSANDRA_DBASE_H
#define CACHEDBCASSANDRA_DBASE_H

#ifdef __cplusplus
extern "C" {
#endif
/* give up on ut.h, don't need it anyway. Also give up on inline stuff */
#define ut_h
#ifdef __cplusplus
#define inline
#endif
#include "../../str.h"
#include "../../dprint.h"
#include "../../mem/mem.h"
#include "../../cachedb/cachedb.h"
#include "../../cachedb/cachedb_cap.h"
#ifdef __cplusplus
#undef inline
#endif

typedef struct {
	struct cachedb_id *id;
	unsigned int ref;
	struct cachedb_pool_con_t *next;

	void *cass_con;
} cassandra_con;

cachedb_con* cassandra_init(str *url);
void cassandra_destroy(cachedb_con *con);
int cassandra_get(cachedb_con *con,str *attr,str *val);
int cassandra_get_counter(cachedb_con *con,str *attr,int *val);
int cassandra_set(cachedb_con *con,str *attr,str *val,int expires);
int cassandra_remove(cachedb_con *con,str *attr);
int cassandra_add(cachedb_con *connection,str *attr,int val,int expires,int *new_val);
int cassandra_sub(cachedb_con *connection,str *attr,int val,int expires,int *new_val);
int cassandra_query_trans(cachedb_con *con,const str *table,const db_key_t* _k, const db_op_t* _op,const db_val_t* _v, const db_key_t* _c, const int _n, const int _nc,const db_key_t _o, db_res_t** _r);
int cassandra_free_result_trans(cachedb_con* con, db_res_t* _r);
int cassandra_insert_trans(cachedb_con *con,const str *table,const db_key_t* _k, const db_val_t* _v,const int _n);
int cassandra_delete_trans(cachedb_con *con,const str *table,const db_key_t* _k,const db_op_t *_o, const db_val_t* _v,const int _n);
int cassandra_update_trans(cachedb_con *con,const str *table,const db_key_t* _k,const db_op_t *_o, const db_val_t* _v,const db_key_t* _uk, const db_val_t* _uv, const int _n,const int _un);
int cassandra_raw_trans(cachedb_con *con, const str *_s, db_res_t** _r);

#ifdef __cplusplus
}
#endif

#endif /* CACHEDBREDIS_DBASE_H */

