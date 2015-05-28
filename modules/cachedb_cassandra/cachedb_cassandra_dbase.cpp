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

#include "cachedb_cassandra.h"
#include "cachedb_cassandra_dbase.h"
#include "cachedb_cassandra_lib.h"
#include <string.h>

void* cassandra_new_connection(char *_host,int port,str *_keyspace,str* _cf,
	str* _counterf)
{
	string host(_host,strlen(_host));
	string keyspace(_keyspace->s,_keyspace->len);
	string cf(_cf->s,_cf->len);
	string counterf(_counterf->s,_counterf->len);

	CassandraConnection *con = new CassandraConnection(keyspace,cf,counterf);
	if (!con) {
		LM_ERR("failed to init CassandraConnection\n");
		return NULL;
	}

	if (con->cassandra_open(host,port,conn_timeout,send_timeout,
		recv_timeout,rd_consistency_level,wr_consistency_level) < 0) {
		LM_ERR("failed to connect to Cassandra DB\n");
		delete con;
		return NULL;
	}

	return (void *)con;
}

void* cassandra_init_connection(struct cachedb_id *id)
{
	cassandra_con *con;
	str keyspace;
	str column_family;
	str counter_family;
	int db_len;
	char *p;

	if (id == NULL) {
		LM_ERR("null cachedb_id\n");
		return 0;
	}

	if (id->flags & CACHEDB_ID_MULTIPLE_HOSTS) {
		LM_ERR("multiple hosts are not supported for cassandra\n");
		return 0;
	}

	if (id->database == NULL) {
		LM_ERR("no database supplied for cassandra\n");
		return 0;
	}

	db_len = strlen(id->database);

	if (db_len == 0) {
		LM_ERR("invalid database. Should be 'keyspace[_columnfamily][_counterfamily]'\n");
		return 0;
	}

	p=(char *)memchr(id->database,'_',db_len);
	if (!p) {
		/* only the keyspace is specified... */
		keyspace.s=id->database;
		keyspace.len=db_len;
                column_family.s = NULL;
                column_family.len = 0;
                counter_family.s = NULL;
                counter_family.len = 0;
	} else {
		keyspace.s=id->database;
		keyspace.len=p-keyspace.s;

		column_family.s=p+1;

		p = (char *)memchr(column_family.s,'_',id->database+db_len-p-1);
		if (!p) {
			LM_ERR("invalid database. Should be 'keyspace_columnfamily_counterfamily'\n");
			return 0;
		}

		column_family.len=p-column_family.s;

		counter_family.s=p+1;
		counter_family.len=id->database+db_len-counter_family.s;
	}

	LM_INFO("Keyspace = [%.*s]. ColumnFamily = [%.*s]. CounterFamily = [%.*s]\n",
		keyspace.len,keyspace.s,column_family.len,column_family.s,
		counter_family.len,counter_family.s);

	con = (cassandra_con *)pkg_malloc(sizeof(cassandra_con));
	if (con == NULL) {
		LM_ERR("no more pkg \n");
		return 0;
	}

	memset(con,0,sizeof(cassandra_con));
	con->id = id;
	con->ref = 1;

	con->cass_con = cassandra_new_connection(id->host,id->port,
		&keyspace,&column_family,&counter_family);

	if (con->cass_con == NULL) {
		LM_ERR("failed to connect to cassandra\n");
		return 0;
	}

	return con;
}

cachedb_con *cassandra_init(str *url)
{
	return cachedb_do_init(url,cassandra_init_connection);
}

void cassandra_free_connection(cachedb_pool_con *con)
{
	cassandra_con * c;
	CassandraConnection *c_con;

	if (!con) return;
	c = (cassandra_con *)con;
	c_con = (CassandraConnection *)c->cass_con;
	c_con->cassandra_close();
	delete c_con;
	pkg_free(c);
	c = NULL;
}

void cassandra_destroy(cachedb_con *con) {
	cachedb_do_close(con,cassandra_free_connection);
}

int cassandra_get(cachedb_con *connection,str *attr,str *val)
{
	cassandra_con *con;
	CassandraConnection *c_con;
	char* col_val;
	int len;
	string col_name(attr->s,attr->len);

	if (!attr || !val || !connection) {
		LM_ERR("null parameter\n");
		return -1;
	}

	con = (cassandra_con *)connection->data;
	c_con = (CassandraConnection *)con->cass_con;

	col_val=(char *)c_con->cassandra_simple_get(col_name).c_str();
	if (col_val == NULL) {
		LM_ERR("failed to fetch Cassandra value\n");
		return -1;
	}

	if (col_val == (char *)-1) {
		LM_DBG("no such key - %.*s\n",attr->len,attr->s);
		val->s = NULL;
		val->len = 0;
		return -2;
	}

	len=strlen(col_val);
	val->s = (char *)pkg_malloc(len);
	if (val->s == NULL) {
		LM_ERR("no more pkg\n");
		return -1;
	}

	val->len=len;
	memcpy(val->s,col_val,len);
	return 0;
}

int cassandra_get_counter(cachedb_con *connection,str *attr,int *val)
{
	cassandra_con *con;
	CassandraConnection *c_con;
	int ret;
	string col_name(attr->s,attr->len);

	if (!attr || !val || !connection) {
		LM_ERR("null parameter\n");
		return -1;
	}

	con = (cassandra_con *)connection->data;
	c_con = (CassandraConnection *)con->cass_con;

	ret=c_con->cassandra_simple_get_counter(col_name,val);
	if (ret < 0) {
		LM_ERR("failed to fetch Cassandra value\n");
		return -1;
	}

	return 0;
}

int cassandra_set(cachedb_con *connection,str *attr,str *val,int expires)
{
	cassandra_con *con;
	CassandraConnection *c_con;
	string col_name(attr->s,attr->len);
	string col_val(val->s,val->len);
	int ret;

	if (!attr || !val || !connection) {
		LM_ERR("null parameter\n");
		return -1;
	}

	con = (cassandra_con *)connection->data;
	c_con = (CassandraConnection *)con->cass_con;

	ret = c_con->cassandra_simple_insert(col_name,col_val,expires);
	if (ret<0) {
		LM_ERR("Failed to insert Cassandra key\n");
		return -1;
	}

	LM_DBG("Succesful cassandra insert\n");
	return 0;
}

int cassandra_remove(cachedb_con *connection,str *attr)
{
	cassandra_con *con;
	CassandraConnection *c_con;
	string col_name(attr->s,attr->len);
	int ret;

	if (!attr || !connection) {
		LM_ERR("null parameter\n");
		return -1;
	}

	con = (cassandra_con *)connection->data;
	c_con = (CassandraConnection *)con->cass_con;

	ret = c_con->cassandra_simple_remove(col_name);
	if (ret<0) {
		LM_ERR("Failed to remove Cassandra key\n");
		return -1;
	}

	LM_DBG("Succesful cassandra remove\n");
	return 0;
}

int cassandra_add(cachedb_con *connection,str *attr,int val,int expires,int *new_val)
{
	cassandra_con *con;
	CassandraConnection *c_con;
	string col_name(attr->s,attr->len);
	int ret;

	if (!attr || !connection) {
		LM_ERR("null parameter\n");
		return -1;
	}

	con = (cassandra_con *)connection->data;
	c_con = (CassandraConnection *)con->cass_con;

	/* TODO - so far no support for expiring counters */
	ret = c_con->cassandra_simple_add(col_name,val);
	if (ret<0) {
		LM_ERR("Failed to add Cassandra key\n");
		return -1;
	}

	LM_DBG("Succesful cassandra add\n");

	ret=c_con->cassandra_simple_get_counter(col_name,new_val);
	if (ret < 0) {
		LM_ERR("failed to fetch Cassandra value\n");
		return -1;
	}

	if (ret > 0) {
		LM_DBG("no such key - %.*s\n",attr->len,attr->s);
		return -2;
	}

	return 0;
}

int cassandra_sub(cachedb_con *connection,str *attr,int val,int expires,int *new_val)
{
	cassandra_con *con;
	CassandraConnection *c_con;
	string col_name(attr->s,attr->len);
	int ret;

	if (!attr || !connection) {
		LM_ERR("null parameter\n");
		return -1;
	}

	con = (cassandra_con *)connection->data;
	c_con = (CassandraConnection *)con->cass_con;

	/* TODO - so far no support for expiring counters */
	ret = c_con->cassandra_simple_sub(col_name,val);
	if (ret<0) {
		LM_ERR("Failed to add Cassandra key\n");
		return -1;
	}

	LM_DBG("Succesful cassandra sub\n");

	ret=c_con->cassandra_simple_get_counter(col_name,new_val);
	if (ret < 0) {
		LM_ERR("failed to fetch Cassandra value\n");
		return -1;
	}

	if (ret > 0) {
		LM_DBG("no such key - %.*s\n",attr->len,attr->s);
		return -2;
	}

	return 0;
}

/**                                                                             
 * \brief Helper function for db queries                                        
 *                                                                              
 * This method evaluates the actual arguments for the database query and        
 * setups the string that is used for the query in the db module.               
 * Then its submit the query and stores the result if necessary. It uses for    
 * its work the implementation in the concrete database module.                 
 *                                                                              
 * \param con structure representing the cachedb_con
 * \param table the table name being requested (must have a column family)
 * \param _k key names, if not present the whole table will be returned         
 * \param _op operators                                                         
 * \param _v values of the keys that must match                                 
 * \param _c column names that should be returned                               
 * \param _n number of key/value pairs that are compared, if zero then no comparison is done
 * \param _nc number of colums that should be returned                          
 * \param _o order by the specificied column, optional                          
 * \param _r the result that is returned                                        
 * \param (*val2str) function pointer to the db specific val conversion function
 * \param (*submit_query) function pointer to the db specific query submit function
 * \param (*store_result) function pointer to the db specific store result function
 * \return zero on success, negative on errors                                  
 */                

/* assumes existing con database
   select table_version from version where table_name = 'location';
   get location['version']

   key = table_name
   value = location

   get version['table_name']['location'] = <table_version>

   select callid, path from location where aor = 'jarrod@ipglobal.net'

   get location['

*/

int cassandra_query_trans(cachedb_con *con,const str *table,const db_key_t* _k, const db_op_t* _op,const db_val_t* _v, const db_key_t* _c, const int _n, const int _nc,const db_key_t _o, db_res_t** _r)
{
       cassandra_con *c;
       CassandraConnection *c_con;
       string family(table->s,table->len);

       string col_name; /*(table->s,table->len);*/

	string col_val;
       int version = -1;
	int j;

        db_row_t *current;
        db_val_t *cur_val;

       if (!_c) {
               /* no column names specified, not good with nosql */
               LM_ERR("The module does not support 'select *' SQL queries \n");
               return -1;
       }

       if (_n == -1) { /* blah */
               LM_ERR("Must only compare one key/value pair\n");
               return -1;
       }

       c = (cassandra_con *)con->data;
       c_con = (CassandraConnection *)c->cass_con;
       c_con->column_family = family;

  	if (strcmp(table->s, "version") == 0) {
		/* row key is key and value is the column
		   so each version is stored in a separate column
		   of the same row :

		   get version['table_name']['location'] = <version>
	         */
       		c_con->column = string(_v[0].val.string_val);
		col_name = string(_k[0]->s, _k[0]->len);	
       		LM_INFO("cassandra get %s[%s][%s];\n",
			 family.c_str(),
			 col_name.c_str(),
			 c_con->column.c_str());
       		version = atoi(c_con->cassandra_simple_get(col_name).c_str());
		LM_INFO("we got this version: %d\n", version);
		//version = atoi(col_val->c_str());
	} else if (strcmp(table->s, "location") == 0) {
       		col_name = string(_v[0].val.string_val); /* dummy_user */
       		col_val=c_con->cassandra_simple_get_slice(col_name);
		*_r = 0;
		return 0;
		//LM_ERR("fix me dummy\n");
		//return -1;
	} else {
               LM_ERR("module unsupported by db_cachedb\n");
               return -1;
	}

       *_r = db_new_result();
       if (*_r == NULL) {
                LM_ERR("Failed to init new result \n");
                goto error;
        }

	/* build the result */
	RES_COL_N(*_r) = _nc;

        /* on first iteration we allocate the result
         * we always assume the query returns exactly the number
         * of 'columns' as were requested */
        if (db_allocate_columns(*_r,_nc) != 0) {
                LM_ERR("Failed to allocate columns \n");
                goto error;
        }

        /* and we initialize the names as if all are there */
        for (j=0;j<_nc;j++) {
                /* since we don't have schema, the types will be allocated
                 * when we fetch the actual rows */
                RES_NAMES(*_r)[j]->s = _c[j]->s;
                RES_NAMES(*_r)[j]->len = _c[j]->len;
        }

        if (db_allocate_rows(*_r,1) != 0) {
                LM_ERR("No more private memory for rows \n");
                goto error;
        }

	RES_ROW_N(*_r) = 1; /* only for version */

	current = &(RES_ROWS(*_r)[0]);	
	ROW_N(current) = RES_COL_N(*_r);
	cur_val = &ROW_VALUES(current)[0];
	memset(cur_val,0,sizeof(db_val_t));
	VAL_TYPE(cur_val) = DB_INT;
	VAL_INT(cur_val) = version;
	
       return 0;

error:
       db_free_result(*_r);
       *_r = NULL;
       return -1;
}

int cassandra_free_result_trans(cachedb_con* con, db_res_t* _r)
{
       //cassandra_con * c;
       //CassandraConnection *c_con;

        if ((!con) || (!_r)) {
                LM_ERR("invalid parameter value\n");
                return -1;
        }

        LM_ERR("freeing cassandra query result \n");

        if (db_free_result(_r) < 0) {
                LM_ERR("unable to free result structure\n");
                return -1;
        }


/*	THE DECONSTRUCTOR hANDLES THIS?? cachedb_cassandra_lib.h:165?
        c = (cassandra_con *)con;
        c_con = (CassandraConnection *)c->cass_con;
        c_con->cassandra_close();
        delete c_con;
        pkg_free(c);
*/
        return 0;
}


int cassandra_insert_trans(cachedb_con *con,const str *table,const db_key_t* _k, const db_val_t* _v,const int _n)
{
	LM_ERR("insert trans\n");
       return 0;
}

int cassandra_delete_trans(cachedb_con *con,const str *table,const db_key_t* _k,const db_op_t *_o, const db_val_t* _v,const int _n)
{
	LM_ERR("delete trans\n");
       return 0;
}

int cassandra_update_trans(cachedb_con *con,const str *table,const db_key_t* _k,const db_op_t *_o, const db_val_t* _v,const db_key_t* _uk, const db_val_t* _uv, const int _n,const int _un)
{
	LM_ERR("update trans\n");
       return 0;
}

int cassandra_raw_trans(cachedb_con *con, const str *_s, db_res_t** _r)
{
	//"cachedb_mongodb_json.c" line 293 of 371 --78%-- col 1
        /* This will most likely never be supported :( */
        LM_ERR("RAW query \n");
	*_r = db_new_result();
	return 0;
}
