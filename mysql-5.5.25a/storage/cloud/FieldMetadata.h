#ifndef FIELD_METADATA_H
#define FIELD_METADATA_H

#include <jni.h>
#include "Util.h"

class FieldMetadata
{
private:
  JNIEnv* env;

  jobject create_java_map()
  {
    jclass map_class = this->env->FindClass("java/util/HashMap");
    jmethodID map_constructor = this->env->GetMethodID(map_class, "<init>", "()V");
    return this->env->NewObject(map_class, map_constructor);
  }

  void java_map_put(jobject map, jobject key, jobject val)
  {
    jclass map_class = this->env->FindClass("java/util/HashMap");
    jmethodID put_method = this->env->GetMethodID(map_class, "put", "(Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;");

    this->env->CallObjectMethod(map, put_method, key, val);
  }

  jobject create_metadata_enum_object(const char *name)
  {
    jclass metadata_class = this->env->FindClass("com/nearinfinity/hbaseclient/ColumnMetadata");
    jfieldID enum_field = this->env->GetStaticFieldID(metadata_class, name, "Lcom/nearinfinity/hbaseclient/ColumnMetadata;");
    jobject enum_object = this->env->GetStaticObjectField(metadata_class, enum_field);

    return enum_object;
  }

  jobject create_column_type_enum_object(const char *name)
  {
    jclass column_type_class = this->env->FindClass("com/nearinfinity/hbaseclient/ColumnType");
    jfieldID enum_field = this->env->GetStaticFieldID(column_type_class, name, "Lcom/nearinfinity/hbaseclient/ColumnType;");
    jobject enum_object = this->env->GetStaticObjectField(column_type_class, enum_field);

    return enum_object;
  }

  jbyteArray type_enum_to_byte_array(const char *name)
  {
    jclass metadata_class = this->env->FindClass("com/nearinfinity/hbaseclient/ColumnType");
    jfieldID enum_field = this->env->GetStaticFieldID(metadata_class, name, "Lcom/nearinfinity/hbaseclient/ColumnType;");
    jobject enum_object = this->env->GetStaticObjectField(metadata_class, enum_field);
    jmethodID get_value_method = this->env->GetMethodID(metadata_class, "getValue", "()[B");

    return (jbyteArray)this->env->CallObjectMethod(enum_object, get_value_method);
  }

  jbyteArray string_to_java_byte_array(const char *string)
  {
    //TODO: Need to free this? - jre
    jstring java_string = this->env->NewStringUTF(string);
    jclass string_class = this->env->FindClass("java/lang/String");
    jmethodID get_bytes_method = this->env->GetMethodID(string_class, "getBytes", "()[B");

    return (jbyteArray)this->env->CallObjectMethod(java_string, get_bytes_method);
  }

  jbyteArray long_to_java_byte_array(longlong val)
  {
    uint array_len = sizeof(longlong);
    //TODO: Need to free this? - jre
    jbyteArray array = this->env->NewByteArray(array_len);
    this->env->SetByteArrayRegion(array, 0, array_len, (jbyte*)&val);

    return array;
  }

public:
  FieldMetadata(JNIEnv* env) : env(env)
  {
  }

  jobject get_field_metadata(Field *field, TABLE *table_arg)
  {
    jclass metadata_class = this->env->FindClass("com/nearinfinity/hbaseclient/ColumnMetadata");

    jmethodID metadata_constructor = this->env->GetMethodID(metadata_class, "<init>", "()V");

    jmethodID set_max_length_method = this->env->GetMethodID(metadata_class, "setMaxLength", "(I)V");
    jmethodID set_precision_method = this->env->GetMethodID(metadata_class, "setPrecision", "(I)V");
    jmethodID set_scale_method = this->env->GetMethodID(metadata_class, "setScale", "(I)V");
    jmethodID set_nullable_method = this->env->GetMethodID(metadata_class, "setNullable", "(Z)V");
    jmethodID set_primary_key_method = this->env->GetMethodID(metadata_class, "setPrimaryKey", "(Z)V");
    jmethodID set_type_method = this->env->GetMethodID(metadata_class, "setType", "(Lcom/nearinfinity/hbaseclient/ColumnType;)V");

    jobject metadata_object = this->env->NewObject(metadata_class, metadata_constructor);

    switch (field->real_type())
    {
      case MYSQL_TYPE_TINY:
      case MYSQL_TYPE_SHORT:
      case MYSQL_TYPE_LONG:
      case MYSQL_TYPE_LONGLONG:
      case MYSQL_TYPE_INT24:
      case MYSQL_TYPE_YEAR:
        if (is_unsigned_field(field))
        {
          this->env->CallVoidMethod(metadata_object, set_type_method, this->create_column_type_enum_object("ULONG"));
        }
        else
        {
          this->env->CallVoidMethod(metadata_object, set_type_method, this->create_column_type_enum_object("LONG"));
        }
        break;
      case MYSQL_TYPE_FLOAT:
      case MYSQL_TYPE_DOUBLE:
        this->env->CallVoidMethod(metadata_object, set_type_method, this->create_column_type_enum_object("DOUBLE"));
          break;
      case MYSQL_TYPE_DECIMAL:
      case MYSQL_TYPE_NEWDECIMAL:
      {
        // TODO: Is this reliable? Field_decimal doesn't seem to have these members. Potential crash for old decimal types. - ABC

        uint precision = ((Field_new_decimal*) field)->precision;
        uint scale = ((Field_new_decimal*) field)->dec;

        this->env->CallVoidMethod(metadata_object, set_type_method, this->create_column_type_enum_object("DECIMAL"));
        this->env->CallVoidMethod(metadata_object, set_precision_method, (jint)precision);
        this->env->CallVoidMethod(metadata_object, set_scale_method, (jint)scale);
      }
        break;
      case MYSQL_TYPE_DATE:
      case MYSQL_TYPE_NEWDATE:
        this->env->CallVoidMethod(metadata_object, set_type_method, this->create_column_type_enum_object("DATE"));
          break;
      case MYSQL_TYPE_TIME:
        this->env->CallVoidMethod(metadata_object, set_type_method, this->create_column_type_enum_object("TIME"));
          break;
      case MYSQL_TYPE_DATETIME:
      case MYSQL_TYPE_TIMESTAMP:
        this->env->CallVoidMethod(metadata_object, set_type_method, this->create_column_type_enum_object("DATETIME"));
          break;
      case MYSQL_TYPE_STRING:
      case MYSQL_TYPE_VARCHAR:
        {
          longlong max_data_length = (longlong)field->max_data_length();

          //If the field might be null, MySQL builds an extra byte into max_data_length. We want to ignore that.
          // UPDATE: In my experience, MySQL always seems to add the extra byte in (reports 256 for VARCHAR(255), etc.) - ABC
          max_data_length--;

          this->env->CallVoidMethod(metadata_object, set_max_length_method, (jint)max_data_length);

          if (field->binary())
          {
            this->env->CallVoidMethod(metadata_object, set_type_method, this->create_column_type_enum_object("BINARY"));
          }
          else
          {
            this->env->CallVoidMethod(metadata_object, set_type_method, this->create_column_type_enum_object("STRING"));
          }
        }
        break;
      case MYSQL_TYPE_BLOB:
      case MYSQL_TYPE_TINY_BLOB:
      case MYSQL_TYPE_MEDIUM_BLOB:
      case MYSQL_TYPE_LONG_BLOB:
        this->env->CallVoidMethod(metadata_object, set_type_method, this->create_column_type_enum_object("BINARY"));
        break;
      case MYSQL_TYPE_ENUM:
        this->env->CallVoidMethod(metadata_object, set_type_method, this->create_column_type_enum_object("ULONG"));
        break;
      case MYSQL_TYPE_NULL:
      case MYSQL_TYPE_BIT:
      case MYSQL_TYPE_SET:
      case MYSQL_TYPE_GEOMETRY:
      case MYSQL_TYPE_VAR_STRING:
      default:
        break;
    }

    if (field->real_maybe_null())
    {
      this->env->CallVoidMethod(metadata_object, set_nullable_method, JNI_TRUE);
    }

    // 64 is obviously some key flag indicating no primary key, but I have no idea where it's defined. Will fix later. - ABC
    if (table_arg->s->primary_key != 64)
    {
      if (strcmp(table_arg->s->key_info[table_arg->s->primary_key].key_part->field->field_name, field->field_name) == 0)
      {
        this->env->CallVoidMethod(metadata_object, set_primary_key_method, JNI_TRUE);
      }
    }

    return metadata_object;
  }
};

#endif
