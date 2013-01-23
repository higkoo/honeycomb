#include "JNISetup.h"
#include <signal.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include "Logging.h"
#include "my_global.h"

#include "Macros.h"
#include "Util.h"
#include "Java.h"
#include "JavaFrame.h"
#include "JNICache.h"

static const char* prefix = "-Djava.class.path=";
static const int prefix_length = strlen(prefix);

static void abort_with_fatal_error(const char* message)
{
  Logging::fatal(message);
  perror(message);
  abort();
}

static int line_length(FILE* option_config)
{
  int ch;
  fpos_t start;
  long int start_pos = ftell(option_config), end_pos = 0;
  if(start_pos < 0) { goto error; }
  if(fgetpos(option_config, &start) != 0) { goto error; }
  do
  {
    ch = fgetc(option_config);
  }
  while(ch != '\n' && ch != EOF);
  end_pos = ftell(option_config);
  if(end_pos < 0) { goto error; }
  if(fsetpos(option_config, &start) != 0) { goto error; }

  return end_pos - start_pos;
error:
  return -1;
}

static int option_count(FILE* option_config)
{
  int ch;
  int count = 0;
  do
  {
    ch = fgetc(option_config);
    if (ch == '\n')
    {
      count++;
    }

  }
  while(ch != EOF);
  if (ferror(option_config)) { goto error; }
  if (fseek(option_config, 0, SEEK_SET) != 0) { goto error; }

  return count;
error:
  return -1;
}

static JavaVMOption* initialize_options(char* class_path, uint* opt_count)
{
  JavaVMOption* options, *option;
  FILE* option_config = fopen("/etc/mysql/jvm-options.conf", "r");
  *opt_count = 1;
  if (option_config != NULL)
  {
    int count = option_count(option_config);
    if(count < 0)
    {
      Logging::warn("Could not successfully count the options in /etc/mysql/jvm-options.conf");
      goto error;
    }

    *opt_count += count;
    options = new JavaVMOption[*opt_count];
    option = options;
    option->optionString = class_path;
    option++;
    int index = 1;
    while(!feof(option_config))
    {
      int line_len = line_length(option_config);
      if (line_len < 0)
      {
        Logging::warn("Line length returned less than 0. Read only %d of %d lines. Not reading the rest of /etc/mysql/jvm-options.conf", index, *opt_count);
        goto error;
      }

      if (line_len == 0 || index >= *opt_count)
      {
        break;
      }

      option->optionString = new char[line_len];
      fgets(option->optionString, line_len, option_config);
      fgetc(option_config); // Skip the newline
      option++;
    }
  }
  else
  {
    Logging::info("No jvm-options.conf found. Using classpath as the only jvm option.");
    options = new JavaVMOption[*opt_count];
    options->optionString = class_path;
  }

error:
  if (option_config)
  {
    fclose(option_config);
  }

  return options;
}

static void destruct(JavaVMOption* options, int option_count)
{
  if(options == NULL)
  {
    return;
  }

  JavaVMOption* option = options;
  for(int i = 0 ; i < option_count ; i++)
  {
    ARRAY_DELETE(option->optionString);
    option++;
  }

  ARRAY_DELETE(options);
}

/**
 * Initialize HBaseAdapter.  This should only be called once per MySQL Server
 * instance during Handlerton initialization.
 */
void initialize_adapter(JavaVM* jvm)
{
  Logging::info("Initializing HBaseAdapter");
  JNIEnv* env;
  jint attach_result = attach_thread(jvm, env);
  if(attach_result != JNI_OK)
  {
    Logging::fatal("Thread could not be attached in initialize_adapter");
    perror("Failed to initalize adapter. Check honeycomb.log for details.");
    abort();
  }
  // TODO: check the result of these JNI calls with macro
  jclass hbase_adapter = env->FindClass(MYSQLENGINE "HBaseAdapter");
  jmethodID initialize = env->GetStaticMethodID(hbase_adapter, "initialize", "()V");
  env->CallStaticVoidMethod(hbase_adapter, initialize);
  if (print_java_exception(env))
  {
    abort_with_fatal_error("Initialize failed with an error. Check"
        "HBaseAdapter.log and honeycomb.log for more information.");
  }
  env->DeleteLocalRef(hbase_adapter);
  detach_thread(jvm);
}

static long file_size(FILE* file)
{
  fseek(file, 0, SEEK_END);
  long size = ftell(file);
  rewind(file);
  return size;
}

static char* read_classpath_conf_file(FILE* config)
{
  char* class_path = NULL, *newline = NULL;
  long class_path_length = 0;
  size_t read_bytes = 0;
  long size = file_size(config);
  if(size <= 0)
  {
    goto error;
  }

  class_path_length = prefix_length + size;
  if (class_path_length < size || class_path_length < 0) // In case the path length wraps around below zero.
  {
    abort_with_fatal_error("The class path is too long.");
  }

  class_path = new char[class_path_length];
  strncpy(class_path, prefix, prefix_length);
  read_bytes = fread(class_path + prefix_length, sizeof(char), size, config);
  if(read_bytes == 0)
  {
    goto error;
  }

  newline = strpbrk(class_path, "\n\r");
  if(newline != NULL)
  {
    *newline = '\0';
  }

  return class_path;
error:
  if(class_path)
  {
    ARRAY_DELETE(class_path);
  }

  return NULL;
}

static char* find_java_classpath()
{
  char* class_path = NULL;
  FILE* config = fopen("/etc/mysql/classpath.conf", "r");
  if(config != NULL)
  {
    Logging::info("Reading the path to HBaseAdapter jar out of /etc/mysql/classpath.conf");
    class_path = read_classpath_conf_file(config);
    fclose(config);
    if (class_path == NULL)
    {
      abort_with_fatal_error("A class path was not found in /etc/mysql/classpath.conf");
    }
  }
  else
  {
    abort_with_fatal_error("Could not open \"classpath.conf\". /etc/mysql/classpath.conf must be readable.");
  }

  Logging::info("Full class path: %s", class_path);
  return class_path;
}

static void print_java_classpath(JNIEnv* env)
{
  Logging::info("Java classpath:");
  jclass classloader_class = env->FindClass("java/lang/ClassLoader");
  jclass url_loader_class = env->FindClass("java/net/URLClassLoader");
  jclass url_class = env->FindClass("java/net/URL");
  jmethodID get_class_method = env->GetStaticMethodID(classloader_class, "getSystemClassLoader", "()Ljava/lang/ClassLoader;");
  jobject class_loader = env->CallStaticObjectMethod(classloader_class, get_class_method);
  jmethodID get_urls_method = env->GetMethodID(url_loader_class, "getURLs", "()[Ljava/net/URL;");
  jmethodID get_file_method = env->GetMethodID(url_class, "getFile", "()Ljava/lang/String;");
  jobjectArray urls = (jobjectArray)env->CallObjectMethod(class_loader, get_urls_method);
  jsize length = env->GetArrayLength(urls);
  for(jsize i = 0; i < length; i++)
  {
    jobject url = env->GetObjectArrayElement(urls, i);
    jstring file = (jstring)env->CallObjectMethod(url, get_file_method);
    const char* string = env->GetStringUTFChars(file, NULL);
    Logging::info("%s", string);
    env->ReleaseStringUTFChars(file, string);
    DELETE_REF(env, file);
    DELETE_REF(env, url);
  }
  DELETE_REF(env, urls);
  DELETE_REF(env, class_loader);
}

extern bool volatile abort_loop;
#if defined(__APPLE__)
extern pthread_handler_t kill_server_thread(void *arg __attribute__((unused)));
static void handler(int sig)
{
  abort_loop = true;
  pthread_t tmp;
  if (mysql_thread_create(0, &tmp, &connection_attrib, kill_server_thread, (void*) &sig))
    sql_print_error("Can't create thread to kill server");
}
#elif defined(__linux__)
extern void kill_mysql(void);
static void handler(int sig)
{
  abort_loop = true;
  kill_mysql();
}
#endif

/**
 * Create an embedded JVM through the JNI Invocation API and calls
 * initialize_adapter. This should only be called once per MySQL Server
 * instance during Handlerton initialization.  Aborts process if a JVM already
 * exists.  After return the current thread is NOT attached.
 */
void initialize_jvm(JavaVM* &jvm)
{
  JavaVM* created_vms;
  jsize vm_count;
  jint result = JNI_GetCreatedJavaVMs(&created_vms, sizeof(created_vms), &vm_count);
  if (result == 0 && vm_count > 0) // There is an existing VM
  {
    abort_with_fatal_error("JVM already created.  Aborting.");
  }
  else
  {
    JNIEnv* env;
    char* class_path = find_java_classpath(); // Stack allocated. Cleaned up in destruct
    JavaVMInitArgs vm_args;
    uint option_count;
    JavaVMOption* options = initialize_options(class_path, &option_count);
    vm_args.options = options;
    vm_args.nOptions = option_count;
    vm_args.version = JNI_VERSION_1_6;
    thread_attach_count++; // roundabout to attach_thread
    jint result = JNI_CreateJavaVM(&jvm, (void**)&env, &vm_args);
    if (result != 0)
    {
      abort_with_fatal_error("Failed to create JVM. Check the Java classpath.");
    }
    if (env == NULL)
    {
      abort_with_fatal_error("Environment not created correctly during JVM creation.");
    }
    initialize_adapter(jvm);
    destruct(options, option_count);
    print_java_classpath(env);
    detach_thread(jvm);
#if defined(__APPLE__) || defined(__linux__)
    signal(SIGTERM, handler);
#endif
  }
}

/**
 * Attach current thread to the JVM.  Assign current environment to env.  Keeps
 * track of how often the current thread has attached, and will not detach
 * until the number of calls to detach is the same as the number of calls to
 * attach.
 *
 * Returns JNI_OK if successful, or a negative number on failure.
 */
jint attach_thread(JavaVM *jvm, JNIEnv* &env)
{
  thread_attach_count++;
  return jvm->AttachCurrentThread((void**) &env, &attach_args);
}

/**
 * Detach thread from JVM.  Will not detach unless the number of calls to
 * detach is the same as the number of calls to attach.
 *
 * Returns JNI_OK if successful, or a negative number on failure.
 */
jint detach_thread(JavaVM *jvm)
{
  thread_attach_count--;

  if(thread_attach_count <= 0)
  {
    thread_attach_count = 0;
    return jvm->DetachCurrentThread();
  }
  else
  {
    return JNI_OK;
  }
}
