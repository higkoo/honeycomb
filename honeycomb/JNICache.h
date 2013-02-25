#ifndef JNICACHE_H
#define JNICACHE_H

#include <jni.h>

/* JNICache holds jmethodID's and jclass global refs to be used in later JNI
 * invocations by Honeycomb.  Upon creation, JNICache asks the JVM for
 * the jmethodID's and jclass refs it needs, and caches them.
 */
class JNICache
{
  public:
    struct HBaseAdapter
    {
      jclass clazz;
      jmethodID initialize,
                create_table,
                get_autoincrement_value,
                alter_autoincrement_value,
                start_write,
                end_write,
                start_scan,
                next_row,
                end_scan,
                write_row,
                update_row,
                flush_writes,
                delete_row,
                delete_all_rows,
                drop_table,
                get_row,
                start_index_scan,
                find_duplicate_key,
                find_duplicate_key_list,
                find_duplicate_value,
                get_next_autoincrement_value,
                index_read,
                next_index_row,
                increment_row_count,
                set_row_count,
                get_row_count,
                rename_table,
                is_nullable,
                add_index,
                drop_index;
    };
    struct IndexReadType
    {
      jclass clazz;
      jfieldID READ_KEY_EXACT,
               READ_AFTER_KEY,
               READ_KEY_OR_NEXT,
               READ_KEY_OR_PREV,
               READ_BEFORE_KEY,
               INDEX_FIRST,
               INDEX_LAST,
               INDEX_NULL;
    };
    struct IndexRow
    {
      jclass clazz;
      jmethodID get_row_map,
                get_uuid;
    };
    struct Row
    {
      jclass clazz;
      jmethodID get_row_map,
                get_uuid;
    };
    struct ColumnMetadata
    {
      jclass clazz;
      jmethodID init,
                set_max_length,
                set_precision,
                set_scale,
                set_nullable,
                set_primary_key,
                set_type,
                set_autoincrement,
                set_autoincrement_value;
    };
    struct ColumnType
    {
      jclass clazz;
      jfieldID NONE,
               STRING,
               BINARY,
               ULONG,
               LONG,
               DOUBLE,
               TIME,
               DATE,
               DATETIME,
               DECIMAL;
    };
    struct KeyValue
    {
      jclass clazz;
      jmethodID init;
    };
    struct TableMultipartKeys
    {
      jclass clazz;
      jmethodID init,
                add_index,
                create_table,
                add_multipart_key;
    };
    struct Throwable
    {
      jclass clazz;
      jmethodID print_stack_trace;
    };
    struct PrintWriter
    {
      jclass clazz;
      jmethodID init;
    };
    struct StringWriter
    {
      jclass clazz;
      jmethodID init,
                to_string;
    };
    struct LinkedList
    {
      jclass clazz;
      jmethodID init,
                add,
                size;
    };
    struct TreeMap
    {
      jclass clazz;
      jmethodID init,
                get,
                put,
                is_empty;
    };

  private:
    JavaVM* jvm;

    HBaseAdapter hbase_adapter_;
    IndexReadType index_read_type_;
    Row row_;
    ColumnMetadata column_metadata_;
    ColumnType column_type_;
    KeyValue key_value_;
    TableMultipartKeys table_multipart_keys_;
    Throwable throwable_;
    PrintWriter print_writer_;
    StringWriter string_writer_;
    LinkedList linked_list_;
    TreeMap tree_map_;

    jclass get_class_ref(JNIEnv* env, const char* clazz);
    jmethodID get_method_id(JNIEnv* env, jclass clazz, const char* method, const char* signature);
    jmethodID get_static_method_id(JNIEnv* env, jclass clazz, const char* method, const char* signature);
    jfieldID get_static_field_id(JNIEnv* env, jclass clazz, const char* field, const char* type);

  public:
    inline HBaseAdapter hbase_adapter()              const{return hbase_adapter_;};
    inline IndexReadType index_read_type()           const{return index_read_type_;};
    inline Row row()                                 const{return row_;};
    inline ColumnMetadata column_metadata()          const{return column_metadata_;};
    inline ColumnType column_type()                  const{return column_type_;};
    inline KeyValue key_value()                      const{return key_value_;};
    inline TableMultipartKeys table_multipart_keys() const{return table_multipart_keys_;};
    inline Throwable throwable()                     const{return throwable_;};
    inline PrintWriter print_writer()                const{return print_writer_;};
    inline StringWriter string_writer()              const{return string_writer_;};
    inline LinkedList linked_list()                  const{return linked_list_;};
    inline TreeMap tree_map()                        const{return tree_map_;};

    JNICache(JavaVM* jvm); 
    ~JNICache();
};

#endif
