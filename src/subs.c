/*
Copyright (c) 2010 Roger Light <roger@atchoo.org>
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice,
   this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.
3. Neither the name of mosquitto nor the names of its
   contributors may be used to endorse or promote products derived from
   this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.
*/

#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <mqtt3.h>
#include <subs.h>
#include <memory_mosq.h>

struct _sub_token {
	struct _sub_token *next;
	char *topic;
};

static int _subs_process(struct _mosquitto_subleaf *leaf, const char *source_id, const char *topic, int qos, int retain, int64_t store_id)
{
	int rc = 0;
	char *client_id;
	int client_qos, msg_qos;
	uint16_t mid;

	while(leaf){
		if(leaf->context->bridge && !strcmp(leaf->context->core.id, source_id)){
			leaf = leaf->next;
			continue;
		}
		client_id = leaf->context->core.id;
		client_qos = leaf->qos;

		if(qos > client_qos){
			msg_qos = client_qos;
		}else{
			msg_qos = qos;
		}
		if(msg_qos){
			mid = mqtt3_db_mid_generate(client_id);
		}else{
			mid = 0;
		}
		switch(msg_qos){
			case 0:
				if(mqtt3_db_message_insert(client_id, mid, mosq_md_out, ms_publish, msg_qos, store_id) == 1) rc = 1;
				break;
			case 1:
				if(mqtt3_db_message_insert(client_id, mid, mosq_md_out, ms_publish_puback, msg_qos, store_id) == 1) rc = 1;
				break;
			case 2:
				if(mqtt3_db_message_insert(client_id, mid, mosq_md_out, ms_publish_pubrec, msg_qos, store_id) == 1) rc = 1;
				break;
		}
		leaf = leaf->next;
	}
	return 0;
}

static int _sub_topic_tokenise(const char *subtopic, struct _sub_token **topics)
{
	struct _sub_token *new_topic, *tail = NULL;
	char *token;
	char *local_subtopic = NULL;

	assert(subtopic);
	assert(topics);

	local_subtopic = _mosquitto_strdup(subtopic);
	if(!local_subtopic) return 1;

	token = strtok(local_subtopic, "/");
	while(token){
		new_topic = _mosquitto_malloc(sizeof(struct _sub_token));
		if(!new_topic) goto cleanup;
		new_topic->next = NULL;
		new_topic->topic = _mosquitto_strdup(token);
		if(!new_topic->topic) goto cleanup;

		if(tail){
			tail->next = new_topic;
			tail = tail->next;
		}else{
			tail = new_topic;
			*topics = tail;
		}
		token = strtok(NULL, "/");
	}
	
	_mosquitto_free(local_subtopic);

	return 0;

cleanup:
	_mosquitto_free(local_subtopic);

	tail = *topics;
	*topics = NULL;
	while(tail){
		if(tail->topic) _mosquitto_free(tail->topic);
		new_topic = tail->next;
		_mosquitto_free(tail);
		tail = new_topic;
	}
	return 1;
}

static int _sub_add(mqtt3_context *context, int qos, struct _mosquitto_subhier *subhier, struct _sub_token *tokens)
{
	struct _mosquitto_subhier *branch, *last = NULL;
	struct _mosquitto_subleaf *leaf, *last_leaf;

	if(!tokens){
		leaf = subhier->subs;
		last_leaf = NULL;
		while(leaf){
			last_leaf = leaf;
			leaf = leaf->next;
		}
		leaf = _mosquitto_malloc(sizeof(struct _mosquitto_subleaf));
		if(!leaf) return 1;
		leaf->next = NULL;
		leaf->context = context;
		leaf->qos = qos;
		if(last_leaf){
			last_leaf->next = leaf;
		}else{
			subhier->subs = leaf;
		}
		return 0;
	}

	branch = subhier->children;
	while(branch){
		if(!strcmp(branch->topic, tokens->topic)){
			return _sub_add(context, qos, branch, tokens->next);
		}
		last = branch;
		branch = branch->next;
	}
	/* Not found */
	branch = _mosquitto_calloc(1, sizeof(struct _mosquitto_subhier));
	if(!branch) return 1;
	if(!last){
		subhier->children = branch;
	}else{
		last->next = branch;
	}
	branch->topic = _mosquitto_strdup(tokens->topic);
	return _sub_add(context, qos, branch, tokens->next);
}

static int _sub_remove(mqtt3_context *context, struct _mosquitto_subhier *subhier, struct _sub_token *tokens)
{
	struct _mosquitto_subhier *branch, *last = NULL;
	struct _mosquitto_subleaf *leaf, *last_leaf;

	if(!tokens){
		leaf = subhier->subs;
		last_leaf = NULL;
		while(leaf){
			if(leaf->context==context){
				if(last_leaf){
					last_leaf->next = leaf->next;
				}else{
					subhier->subs = leaf->next;
				}
				_mosquitto_free(leaf);
				return 0;
			}
			last_leaf = leaf;
			leaf = leaf->next;
		}
		return 0;
	}

	branch = subhier->children;
	while(branch){
		if(!strcmp(branch->topic, tokens->topic)){
			_sub_remove(context, branch, tokens->next);
			if(!branch->children && !branch->subs){
				if(last){
					last->next = branch->next;
				}else{
					subhier->children = branch->next;
				}
				_mosquitto_free(branch->topic);
				_mosquitto_free(branch);
			}
			return 0;
		}
		last = branch;
		branch = branch->next;
	}
	return 0;
}

static int _sub_search(struct _mosquitto_subhier *subhier, struct _sub_token *tokens, const char *source_id, const char *topic, int qos, int retain, int64_t store_id)
{
	/* FIXME - need to take into account source_id if the client is a bridge */
	struct _mosquitto_subhier *branch, *last = NULL;

	branch = subhier->children;
	while(branch){
		if(!strcmp(branch->topic, tokens->topic) || !strcmp(branch->topic, "+")){
			/* The topic matches this subscription.
			 * Doesn't include # wildcards */
			if(tokens->next){
				_sub_search(branch, tokens->next, source_id, topic, qos, retain, store_id);
			}else{
				_subs_process(branch->subs, source_id, topic, qos, retain, store_id);
			}
		}else if(!strcmp(branch->topic, "#") && !branch->children){
			/* The topic matches due to a # wildcard - process the
			 * subscriptions and return. */
			_subs_process(branch->subs, source_id, topic, qos, retain, store_id);
			return 0;
		}
		last = branch;
		branch = branch->next;
	}
	return 0;
}

int mqtt3_sub_add(mqtt3_context *context, int qos, struct _mosquitto_subhier *root, const char *sub)
{
	int tree;
	int rc = 0;
	struct _mosquitto_subhier *subhier;
	struct _sub_token *tokens = NULL, *tail;

	assert(root);
	assert(sub);

	if(!strncmp(sub, "$SYS", 4)){
		tree = 2;
		if(_sub_topic_tokenise(sub, &tokens)) return 1;
	}else if(sub[0] == '/'){
		tree = 1;
		if(_sub_topic_tokenise(sub+1, &tokens)) return 1;
	}else{
		tree = 0;
		if(_sub_topic_tokenise(sub, &tokens)) return 1;
	}

	subhier = root->children;
	while(subhier){
		if(!strcmp(subhier->topic, "") && tree == 0){
			rc = _sub_add(context, qos, subhier, tokens);
			break;
		}else if(!strcmp(subhier->topic, "/") && tree == 1){
			rc = _sub_add(context, qos, subhier, tokens);
			break;
		}else if(!strcmp(subhier->topic, "$SYS") && tree == 2){
			rc = _sub_add(context, qos, subhier, tokens);
			break;
		}
		subhier = subhier->next;
	}

	while(tokens){
		tail = tokens->next;
		_mosquitto_free(tokens->topic);
		_mosquitto_free(tokens);
		tokens = tail;
	}
	return rc;
}

int mqtt3_sub_remove(mqtt3_context *context, struct _mosquitto_subhier *root, const char *sub)
{
	int rc = 0;
	int tree;
	struct _mosquitto_subhier *subhier;
	struct _sub_token *tokens = NULL, *tail;

	assert(root);
	assert(sub);

	if(!strncmp(sub, "$SYS", 4)){
		tree = 2;
		if(_sub_topic_tokenise(sub, &tokens)) return 1;
	}else if(sub[0] == '/'){
		tree = 1;
		if(_sub_topic_tokenise(sub, &tokens)) return 1;
	}else{
		tree = 0;
		if(_sub_topic_tokenise(sub, &tokens)) return 1;
	}

	subhier = root->children;
	while(subhier){
		if(!strcmp(subhier->topic, "") && tree == 0){
			rc = _sub_remove(context, subhier, tokens);
			break;
		}else if(!strcmp(subhier->topic, "/") && tree == 1){
			rc = _sub_remove(context, subhier, tokens);
			break;
		}else if(!strcmp(subhier->topic, "$SYS") && tree == 2){
			rc = _sub_remove(context, subhier, tokens);
			break;
		}
		subhier = subhier->next;
	}

	while(tokens){
		tail = tokens->next;
		_mosquitto_free(tokens->topic);
		_mosquitto_free(tokens);
		tokens = tail;
	}

	return rc;
}

int mqtt3_sub_search(struct _mosquitto_subhier *root, const char *source_id, const char *topic, int qos, int retain, int64_t store_id)
{
	int rc = 0;
	int tree;
	struct _mosquitto_subhier *subhier;
	struct _sub_token *tokens = NULL, *tail;

	assert(root);
	assert(topic);

	if(!strncmp(topic, "$SYS", 4)){
		tree = 2;
		if(_sub_topic_tokenise(topic, &tokens)) return 1;
	}else if(topic[0] == '/'){
		tree = 1;
		if(_sub_topic_tokenise(topic+1, &tokens)) return 1;
	}else{
		tree = 0;
		if(_sub_topic_tokenise(topic, &tokens)) return 1;
	}

	subhier = root->children;
	while(subhier){
		if(!strcmp(subhier->topic, "") && tree == 0){
			rc = _sub_search(subhier, tokens, source_id, topic, qos, retain, store_id);
		}else if(!strcmp(subhier->topic, "/") && tree == 1){
			rc = _sub_search(subhier, tokens, source_id, topic, qos, retain, store_id);
		}else if(!strcmp(subhier->topic, "$SYS") && tree == 2){
			rc = _sub_search(subhier, tokens, source_id, topic, qos, retain, store_id);
		}
		subhier = subhier->next;
	}
	while(tokens){
		tail = tokens->next;
		_mosquitto_free(tokens->topic);
		_mosquitto_free(tokens);
		tokens = tail;
	}

	return rc;
}

void mqtt3_sub_tree_print(struct _mosquitto_subhier *root, int level)
{
	int i;
	struct _mosquitto_subhier *branch;
	struct _mosquitto_subleaf *leaf;

	for(i=0; i<level*2; i++){
		printf(" ");
	}
	printf("%s", root->topic);
	leaf = root->subs;
	while(leaf){
		printf(" (%s, %d)", "", leaf->qos);
		leaf = leaf->next;
	}
	printf("\n");

	branch = root->children;
	while(branch){
		mqtt3_sub_tree_print(branch, level+1);
		branch = branch->next;
	}
}
