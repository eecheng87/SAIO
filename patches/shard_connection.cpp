--- patch/memtier_benchmark-1.4.0/shard_connection.cpp	2022-08-08 09:50:50.000000000 +0000
+++ patch2/memtier_benchmark-1.4.0/shard_connection.cpp	2022-08-26 13:47:20.538667813 +0000
@@ -329,7 +329,11 @@
     m_pipeline->pop();
 
     m_pending_resp--;
-    assert(m_pending_resp >= 0);
+    if(m_pending_resp < 0){
+        m_pending_resp = 0;
+        return NULL;
+    }
+
 
     return req;
 }
@@ -392,7 +396,14 @@
         protocol_response *r = m_protocol->get_response();
 
         request* req = pop_req();
-        switch (req->m_type)
+        
+	if(!req) {
+            responses_handled = true;
+            delete req;
+            break;
+        }
+
+	switch (req->m_type)
         {
         case rt_auth:
             if (r->is_error()) {
@@ -481,6 +492,8 @@
     }
 
     if (m_conns_manager->finished()) {
+        m_pending_resp = 0;
+        bufferevent_disable(m_bev, EV_WRITE|EV_READ);
         m_conns_manager->set_end_time();
     }
 }
