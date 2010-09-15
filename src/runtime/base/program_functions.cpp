/*
   +----------------------------------------------------------------------+
   | HipHop for PHP                                                       |
   +----------------------------------------------------------------------+
   | Copyright (c) 2010 Facebook, Inc. (http://www.facebook.com)          |
   +----------------------------------------------------------------------+
   | This source file is subject to version 3.01 of the PHP license,      |
   | that is bundled with this package in the file LICENSE, and is        |
   | available through the world-wide-web at the following url:           |
   | http://www.php.net/license/3_01.txt                                  |
   | If you did not receive a copy of the PHP license and are unable to   |
   | obtain it through the world-wide-web, please send a note to          |
   | license@php.net so we can mail you a copy immediately.               |
   +----------------------------------------------------------------------+
*/

#include <runtime/base/types.h>
#include <runtime/base/program_functions.h>
#include <runtime/base/builtin_functions.h>
#include <runtime/base/execution_context.h>
#include <runtime/base/runtime_option.h>
#include <util/shared_memory_allocator.h>
#include <system/gen/sys/system_globals.h>
#include <system/gen/php/globals/symbols.h>
#include <runtime/base/server/pagelet_server.h>
#include <runtime/base/server/xbox_server.h>
#include <runtime/base/server/http_server.h>
#include <runtime/base/server/replay_transport.h>
#include <runtime/base/server/http_request_handler.h>
#include <runtime/base/server/admin_request_handler.h>
#include <runtime/base/server/server_stats.h>
#include <runtime/base/server/server_note.h>
#include <runtime/base/memory/memory_manager.h>
#include <util/process.h>
#include <util/capability.h>
#include <util/timer.h>
#include <util/stack_trace.h>
#include <util/light_process.h>
#include <runtime/base/source_info.h>
#include <runtime/base/rtti_info.h>
#include <runtime/base/frame_injection.h>
#include <runtime/ext/extension.h>
#include <runtime/ext/ext_fb.h>
#include <runtime/ext/ext_json.h>
#include <runtime/ext/ext_variable.h>
#include <runtime/ext/ext_apc.h>
#include <runtime/eval/runtime/code_coverage.h>
#include <runtime/eval/debugger/debugger.h>
#include <runtime/eval/debugger/debugger_client.h>
#include <runtime/base/fiber_async_func.h>
#include <runtime/base/util/simple_counter.h>

#include <boost/program_options/options_description.hpp>
#include <boost/program_options/positional_options.hpp>
#include <boost/program_options/variables_map.hpp>
#include <boost/program_options/parsers.hpp>
#include <libgen.h>

#include <runtime/eval/runtime/eval_state.h>

using namespace std;
using namespace boost::program_options;
extern char **environ;

#define MAX_INPUT_NESTING_LEVEL 64

namespace HPHP {
///////////////////////////////////////////////////////////////////////////////
// helpers

struct ProgramOptions {
  string     mode;
  string     config;
  StringVec  confStrings;
  int        port;
  int        admin_port;
  string     user;
  string     file;
  int        count;
  bool       noSafeAccessCheck;
  StringVec  args;
  string     buildId;
  int        xhprofFlags;

  Eval::DebuggerClientOptions debugger_options;
};

class StartTime {
public:
  StartTime() : startTime(time(NULL)) {}
  time_t startTime;
};
static StartTime s_startTime;

time_t start_time() {
  return s_startTime.startTime;
}

static void process_cmd_arguments(int argc, char **argv) {
  SystemGlobals *g = (SystemGlobals *)get_global_variables();
  g->gv_argc = argc;
  for (int i = 0; i < argc; i++) {
    g->gv_argv.lvalAt() = argv[i];
  }
}

void process_env_variables(Variant &variables) {
  for (map<string, string>::const_iterator iter =
         RuntimeOption::EnvVariables.begin();
       iter != RuntimeOption::EnvVariables.end(); ++iter) {
    variables.set(String(iter->first), String(iter->second));
  }
  for (char **env = environ; env && *env; env++) {
    char *p = strchr(*env, '=');
    if (p) {
      String name(*env, p - *env, CopyString);
      register_variable(variables, (char*)name.data(),
                        String(p + 1, CopyString));
    }
  }
}

void register_variable(Variant &variables, char *name, CVarRef value,
                       bool overwrite /* = true */) {
  // ignore leading spaces in the variable name
  char *var = name;
  while (*var && *var == ' ') {
    var++;
  }

  // ensure that we don't have spaces or dots in the variable name
  // (not binary safe)
  bool is_array = false;
  char *ip = NULL; // index pointer
  char *p = var;
  for (; *p; p++) {
    if (*p == ' ' || *p == '.') {
      *p = '_';
    } else if (*p == '[') {
      is_array = true;
      ip = p;
      *p = 0;
      break;
    }
  }
  int var_len = p - var;
  if (var_len == 0) {
    // empty variable name, or variable name with a space in it
    return;
  }

  vector<Variant> gpc_elements;
  gpc_elements.reserve(MAX_INPUT_NESTING_LEVEL); // important, so no resize
  Variant *symtable = &variables;
  char *index = var;
  int index_len = var_len;

  if (is_array) {
    int nest_level = 0;
    while (true) {
      if (++nest_level > MAX_INPUT_NESTING_LEVEL) {
        Logger::Warning("Input variable nesting level exceeded");
        return;
      }

      ip++;
      char *index_s = ip;
      int new_idx_len = 0;
      if (isspace(*ip)) {
        ip++;
      }
      if (*ip == ']') {
        index_s = NULL;
      } else {
        ip = strchr(ip, ']');
        if (!ip) {
          // PHP variables cannot contain '[' in their names,
          // so we replace the character with a '_'
          *(index_s - 1) = '_';

          index_len = 0;
          if (index) {
            index_len = strlen(index);
          }
          goto plain_var;
        }
        *ip = 0;
        new_idx_len = strlen(index_s);
      }

      if (!index) {
        symtable->append(Array::Create());
        gpc_elements.push_back(null);
        gpc_elements.back() =
          ref(symtable->lvalAt((int)symtable->toArray().size()));
      } else {
        String key(index, index_len, CopyString);
        Variant v = symtable->rvalAt(key);
        if (v.isNull() || !v.is(KindOfArray)) {
          symtable->set(key, Array::Create());
        }
        gpc_elements.push_back(null);
        gpc_elements.back() = ref(symtable->lvalAt(key));
      }
      symtable = &gpc_elements.back();
      /* ip pointed to the '[' character, now obtain the key */
      index = index_s;
      index_len = new_idx_len;

      ip++;
      if (*ip == '[') {
        is_array = true;
        *ip = 0;
      } else {
        goto plain_var;
      }
    }
  } else {
  plain_var:
    if (!index) {
      symtable->append(value);
    } else {
      String key(index, index_len, CopyString);
      if (overwrite || !symtable->toArray().exists(key)) {
        symtable->set(key, value);
      }
    }
  }
}

enum ContextOfException {
  WarmupDocException = 0,
  ReqInitException,
  InvokeException,
  HandlerException,
};

static bool handle_exception(ExecutionContext *context, std::string &errorMsg,
                             ContextOfException where, bool &error) {
  bool ret = false;
  try {
    throw;
  } catch (const Eval::DebuggerException &e) {
    throw;
  } catch (const ExitException &e) {
    ret = true;
    // ExitException is fine
  } catch (const PhpFileDoesNotExistException &e) {
    if (where == WarmupDocException) {
      Logger::Error("warmup error: %s", e.getMessage().c_str());
    }
  } catch (const UncatchableException &e) {
    if (RuntimeOption::ServerStackTrace) {
      errorMsg = e.what();
    } else {
      errorMsg = e.getStackTrace().hexEncode();
      errorMsg += " ";
      errorMsg += e.getMessage();
    }
    Logger::Error("%s", errorMsg.c_str());
    error = true;
  } catch (const Exception &e) {
    if (where == HandlerException) {
      errorMsg = "Exception handler threw an exception: ";
    }
    if (RuntimeOption::ServerStackTrace) {
      errorMsg += e.what();
    } else {
      errorMsg += e.getStackTrace().hexEncode();
      errorMsg += " ";
      errorMsg += e.getMessage();
    }
    if (where == InvokeException) {
      ret = context->onFatalError(e);
      error = !ret;
    } else {
      error = true;
      Logger::Error("%s", errorMsg.c_str());
    }
  } catch (const Object &e) {
    if (where == HandlerException) {
      errorMsg = "Exception handler threw an object exception: ";
    }
    try {
      errorMsg += e.toString().data();
    } catch (...) {
      errorMsg += "(unable to call toString())";
    }
    if (where == InvokeException) {
      context->onUnhandledException(e);
    } else {
      Logger::Error("%s", errorMsg.c_str());
    }
    error = true;
  } catch (...) {
    if (where == InvokeException) throw;
    errorMsg = "(unknown exception was thrown)";
    Logger::Error("%s", errorMsg.c_str());
    error = true;
  }
  return ret;
}

static bool hphp_chdir_file(const string filename) {
  bool ret = false;
  String s = File::TranslatePath(filename);
  char *buf = strndup(s.data(), s.size());
  char *dir = dirname(buf);
  ASSERT(dir);
  if (dir) {
    if (File::IsVirtualDirectory(dir)) {
      g_context->setCwd(String(dir, CopyString));
      ret = true;
    } else {
      struct stat sb;
      stat(dir, &sb);
      if ((sb.st_mode & S_IFMT) == S_IFDIR) {
        ret = true;
        if (*dir != '.') {
          g_context->setCwd(String(dir, CopyString));
        }
      }
    }
  }
  free(buf);
  return ret;
}

void handle_destructor_exception() {
  string errorMsg;
  try {
    throw;
  } catch (ExitException &e) {
    // ExitException is fine
  } catch (Exception &e) {
    errorMsg = "Destructor threw an exception: ";
    if (RuntimeOption::ServerStackTrace) {
      errorMsg += e.what();
    } else {
      errorMsg += e.getStackTrace().hexEncode();
      errorMsg += " ";
      errorMsg += e.getMessage();
    }
    Logger::Error("%s", errorMsg.c_str());
  } catch (Object &e) {
    errorMsg = "Destructor threw an object exception: ";
    try {
      errorMsg += e.toString().data();
    } catch (...) {
      errorMsg += "(unable to call toString())";
    }
    Logger::Error("%s", errorMsg.c_str());
  } catch (...) {
    errorMsg = "(unknown exception was thrown from destructor)";
    Logger::Error("%s", errorMsg.c_str());
  }
}

void execute_command_line_begin(int argc, char **argv, int xhprof) {
  hphp_session_init();
  ExecutionContext *context = g_context.get();
  context->obSetImplicitFlush(true);

  SystemGlobals *g = (SystemGlobals *)get_global_variables();

  // reset global symbols to nulls or empty arrays
  pm_php$globals$symbols_php();

  process_env_variables(g->gv__ENV);
  g->gv__ENV.set("HPHP", 1);

  process_cmd_arguments(argc, argv);

  Variant &server = g->gv__SERVER;
  process_env_variables(server);
  server.set("DOCUMENT_ROOT", "");
  server.set("SCRIPT_FILENAME", argv[0]);
  server.set("SCRIPT_NAME", argv[0]);
  server.set("PHP_SELF", argv[0]);
  server.set("argv", g->gv_argv);
  server.set("argc", g->gv_argc);
  server.set("PWD", g_context->getCwd());
  char hostname[1024];
  if (!gethostname(hostname, 1024)) {
    server.set("HOSTNAME", String(hostname, CopyString));
  }

  for(std::map<string,string>::iterator it =
        RuntimeOption::ServerVariables.begin(),
        end = RuntimeOption::ServerVariables.end(); it != end; ++it) {
    server.set(String(it->first.c_str()), String(it->second.c_str()));
  }

  if (RuntimeOption::EnableCliRTTI) RTTIInfo::TheRTTIInfo.init(true);

  if (xhprof) {
    f_xhprof_enable(xhprof, null);
  }
}

void execute_command_line_end(int xhprof, bool coverage, const char *program) {
  if (xhprof) {
    f_var_dump(f_json_encode(f_xhprof_disable()));
  }
  hphp_context_exit(g_context.get(), true, true, program);
  hphp_session_exit();
  if (coverage && RuntimeOption::RecordCodeCoverage &&
      !RuntimeOption::CodeCoverageOutputFile.empty()) {
    Eval::CodeCoverage::Report(RuntimeOption::CodeCoverageOutputFile);
  }
}

static void pagein_self(void) {
  unsigned long begin, end, inode, pgoff;
  char mapname[PATH_MAX];
  char perm[5];
  char dev[6];
  char *buf;
  int bufsz;
  int r;
  FILE *fp;

  // pad due to the spaces between the inode number and the mapname
  bufsz = sizeof(unsigned long) * 4 + sizeof(mapname) + sizeof(char) * 11 + 100;
  buf = (char *)malloc(bufsz);
  if (buf == NULL)
    return;

  Timer timer(Timer::WallTime, "mapping self");
  fp = fopen("/proc/self/maps", "r");
  if (fp != NULL) {
    while (!feof(fp)) {
      if (fgets(buf, bufsz, fp) == 0)
        break;
      r = sscanf(buf, "%lx-%lx %4s %lx %5s %ld %s",
                 &begin, &end, perm, &pgoff, dev, &inode, mapname);

      // page in read-only segments that correspond to a file on disk
      if (r != 7 ||
          perm[0] != 'r' ||
          perm[1] != '-' ||
          access(mapname, F_OK) != 0) {
        continue;
      }

      if (mlock((void *)begin, end - begin) == 0)
        munlock((void *)begin, end - begin);
    }
    fclose(fp);
  }
  free(buf);
}

static int start_server(const std::string &username) {
  // Before we start the webserver, make sure the entire
  // binary is paged into memory.
  pagein_self();

  RuntimeOption::ExecutionMode = "srv";
  HttpRequestHandler::GetAccessLog().init
    (RuntimeOption::AccessLogDefaultFormat, RuntimeOption::AccessLogs);
  AdminRequestHandler::GetAccessLog().init
    (RuntimeOption::AdminLogFormat, RuntimeOption::AdminLogFile);

#if !defined(SKIP_USER_CHANGE)
  if (!username.empty()) {
    Capability::ChangeUnixUser(username);
    LightProcess::ChangeUser(username);
  }
#endif

  HttpServer::Server = HttpServerPtr(new HttpServer());
  HttpServer::Server->run();
  return 0;
}

string translate_stack(const char *hexencoded, bool with_frame_numbers) {
  if (!hexencoded || !*hexencoded) {
    return "";
  }

  StackTrace st(hexencoded);
  StackTrace::FramePtrVec frames;
  st.get(frames);

  ostringstream out;
  for (unsigned int i = 0; i < frames.size(); i++) {
    StackTrace::FramePtr f = frames[i];
    if (with_frame_numbers) {
      out << "# " << (i < 10 ? " " : "") << i << ' ';
    }
    out << f->toString();
    if (SourceInfo::TheSourceInfo.translate(f)) {
      out << " [" << f->filename << ':' << f->lineno << ']';
    }
    out << '\n';
  }
  return out.str();
}

void translate_rtti(const char *rttiDirectory) {
  RTTIInfo::TheRTTIInfo.translate_rtti(rttiDirectory);
}

///////////////////////////////////////////////////////////////////////////////

static void prepare_args(int &argc, char **&argv, const StringVec &args,
                         const char *file) {
  argv = (char **)malloc((args.size() + 2) * sizeof(char*));
  argc = 0;
  if (file) {
    argv[argc++] = (char*)file;
  }
  for (int i = 0; i < (int)args.size(); i++) {
    argv[argc++] = (char*)args[i].c_str();
  }
  argv[argc] = NULL;
}

static int execute_program_impl(int argc, char **argv);
int execute_program(int argc, char **argv) {
  try {
    return execute_program_impl(argc, argv);
  } catch (const Exception &e) {
    cerr << "Uncaught exception: " << e.what();
  } catch (const std::exception &e) {
    cerr << "Uncaught exception: " << e.what();
  } catch (...) {
    cerr << "Uncaught exception: (unknown)\n";
  }
  return -1;
}

static int execute_program_impl(int argc, char **argv) {
  string usage = "Usage:\n\n\t";
  usage += argv[0];
  usage += " [-m <mode>] [<options>] [<arg1>] [<arg2>] ...\n\nOptions";

  ProgramOptions po;
  options_description desc(usage.c_str());
  desc.add_options()
    ("help", "display this message")
#ifdef COMPILER_ID
    ("compiler-id", "display the git hash for the compiler id")
#endif
    ("mode,m", value<string>(&po.mode)->default_value("run"),
     "run | debug (d) | server (s) | daemon | replay | translate (t)")
    ("config,c", value<string>(&po.config),
     "load specified config file")
    ("config-value,v", value<StringVec >(&po.confStrings)->composing(),
     "individual configuration string in a format of name=value, where "
     "name can be any valid configuration for a config file")
    ("port,p", value<int>(&po.port)->default_value(-1),
     "start an HTTP server at specified port")
    ("admin-port", value<int>(&po.admin_port)->default_value(-1),
     "start admin listerner at specified port")
    ("debug-host,h", value<string>(&po.debugger_options.host),
     "connect to debugger server at specified address")
    ("debug-port", value<int>(&po.debugger_options.port)->default_value(-1),
     "connect to debugger server at specified port")
    ("debug-extension", value<string>(&po.debugger_options.extension),
     "PHP file that extends y command")
    ("debug-cmd", value<StringVec>(&po.debugger_options.cmds)->composing(),
     "executes this debugger command and returns its output in stdout")
    ("debug-sandbox",
     value<string>(&po.debugger_options.sandbox)->default_value("default"),
     "initial sandbox to attach to when debugger is started")
    ("user,u", value<string>(&po.user),
     "run server under this user account")
    ("file,f", value<string>(&po.file),
     "executing specified file")
    ("count", value<int>(&po.count)->default_value(1),
     "how many times to repeat execution")
    ("no-safe-access-check",
      value<bool>(&po.noSafeAccessCheck)->default_value(false),
     "whether to ignore safe file access check")
    ("arg", value<StringVec >(&po.args)->composing(),
     "arguments")
    ("extra-header", value<string>(&Logger::ExtraHeader),
     "extra-header to add to log lines")
    ("build-id", value<string>(&po.buildId),
     "unique identifier of compiled server code")
    ("xhprof-flags", value<int>(&po.xhprofFlags)->default_value(0),
     "Set XHProf flags")
    ;

  positional_options_description p;
  p.add("arg", -1);
  variables_map vm;
  try {
    store(command_line_parser(argc, argv).options(desc).positional(p).run(),
          vm);
    notify(vm);
  } catch (error &e) {
    cerr << "Error in command line: " << e.what() << "\n\n";
    cout << desc << "\n";
    return -1;
  } catch (...) {
    cerr << "Error in command line:\n\n";
    cout << desc << "\n";
    return -1;
  }
  if (vm.count("help")) {
    cout << desc << "\n";
    return -1;
  }
#ifdef COMPILER_ID
  if (vm.count("compiler-id")) {
    cout << COMPILER_ID << "\n";
    return 0;
  }
#endif

  Hdf config;
  if (!po.config.empty()) {
    config.open(po.config);
  }
  for (unsigned int i = 0; i < po.confStrings.size(); i++) {
    config.fromString(po.confStrings[i].c_str());
  }
  RuntimeOption::Load(config);
  vector<string> badnodes;
  config.lint(badnodes);
  for (unsigned int i = 0; i < badnodes.size(); i++) {
    Logger::Error("Possible bad config node: %s", badnodes[i].c_str());
  }

  LightProcess::Initialize(RuntimeOption::LightProcessFilePrefix,
                           RuntimeOption::LightProcessCount);

  RuntimeOption::BuildId = po.buildId;
  if (po.port != -1) {
    RuntimeOption::ServerPort = po.port;
  }
  if (po.admin_port != -1) {
    RuntimeOption::AdminServerPort = po.admin_port;
  }
  if (po.noSafeAccessCheck) {
    RuntimeOption::SafeFileAccess = false;
  }

  if (!RuntimeOption::LogFile.empty()) {
    if (RuntimeOption::LogFile[0] == '|') {
      Logger::Output = popen(RuntimeOption::LogFile.substr(1).c_str(), "w");
    } else {
      Logger::Output = fopen(RuntimeOption::LogFile.c_str(), "w");
    }
  }

  if (po.mode == "d") po.mode = "debug";
  if (po.mode == "s") po.mode = "server";
  if (po.mode == "t") po.mode = "translate";

  MethodIndexHMap::initialize(false);
  if (argc <= 1 || po.mode == "run" || po.mode == "debug") {
    RuntimeOption::ExecutionMode = "cli";

    int new_argc;
    char **new_argv;
    prepare_args(new_argc, new_argv, po.args, po.file.c_str());

    int ret = 0;
    hphp_process_init();

    if (po.mode == "debug") {
      RuntimeOption::EnableDebugger = true;
      Eval::Debugger::StartClient(po.debugger_options);

      string file = po.file;
      StringVecPtr client_args; bool restarting = false;
      ret = 0;
      while (true) {
        try {
          execute_command_line_begin(new_argc, new_argv, po.xhprofFlags);

          DECLARE_THREAD_INFO;
          FRAME_INJECTION_FLAGS(empty_string, _, FrameInjection::PseudoMain);

          if (po.debugger_options.extension.empty()) {
            // even if it's empty, still need to call for warmup
            hphp_invoke_simple(" "); // so not to run the 1st file if compiled
          } else {
            hphp_invoke_simple(po.debugger_options.extension);
          }
          Eval::Debugger::RegisterSandbox(Eval::DSandboxInfo());
          if (!restarting) {
            Eval::Debugger::InterruptSessionStarted(new_argv[0]);
          }
          hphp_invoke_simple(file);
          Eval::Debugger::InterruptSessionEnded(new_argv[0]);
          execute_command_line_end(po.xhprofFlags, true, new_argv[0]);
        } catch (const Eval::DebuggerRestartException &e) {
          execute_command_line_end(0, false, NULL);

          if (!e.m_args->empty()) {
            file = e.m_args->at(0);
            client_args = e.m_args;
            free(new_argv);
            prepare_args(new_argc, new_argv, *client_args, NULL);
          }
          restarting = true;
        } catch (const Eval::DebuggerClientExitException &e) {
          execute_command_line_end(0, false, NULL);
          break; // end user quitting debugger
        }
      }

    } else {
      ret = 0;
      for (int i = 0; i < po.count; i++) {
        execute_command_line_begin(new_argc, new_argv, po.xhprofFlags);
        ret = -1;
        if (hphp_invoke_simple(po.file)) {
          ret = ExitException::ExitCode;
        }
        execute_command_line_end(po.xhprofFlags, true, new_argv[0]);
      }
    }

    free(new_argv);
    hphp_process_exit();
    return ret;
  }

  if (po.mode == "daemon") {
    Process::Daemonize();
    return start_server(po.user);
  }
  if (po.mode == "server") {
    return start_server(po.user);
  }

  if (po.mode == "replay" && !po.args.empty()) {
    RuntimeOption::RecordInput = false;
    RuntimeOption::ExecutionMode = "srv";
    HttpServer server; // so we initialize runtime properly
    HttpRequestHandler handler;
    for (int i = 0; i < po.count; i++) {
      for (unsigned int j = 0; j < po.args.size(); j++) {
        ReplayTransport rt;
        rt.replayInput(po.args[j].c_str());
        handler.handleRequest(&rt);
        printf("%s\n", rt.getResponse().c_str());
      }
    }
    return 0;
  }

  if (po.mode == "translate" && !po.args.empty()) {
    if (!access(po.args[0].c_str(), F_OK)) {
      translate_rtti(po.args[0].c_str());
    } else {
      printf("%s", translate_stack(po.args[0].c_str()).c_str());
    }
    return 0;
  }

  cout << desc << "\n";
  return -1;
}

String canonicalize_path(CStrRef p, const char* root, int rootLen) {
  String path(Util::canonicalize(p.c_str(), p.size()), AttachString);
  if (path.charAt(0) == '/') {
    string &sourceRoot = RuntimeOption::SourceRoot;
    int len = sourceRoot.size();
    if (len && strncmp(path.data(), sourceRoot.c_str(), len) == 0) {
      return path.substr(len);
    }
    if (root && rootLen && strncmp(path.data(), root, rootLen) == 0) {
      return path.substr(rootLen);
    }
  }
  return path;
}

///////////////////////////////////////////////////////////////////////////////
// C++ ffi

class WarmupState {
public:
  WarmupState() : done(false), enabled(false),
                  atCheckpoint(false), failed(false) {}
  bool done;
  bool enabled;
  bool atCheckpoint;
  bool failed;
};
static IMPLEMENT_THREAD_LOCAL(WarmupState, s_warmup_state);

void hphp_process_init() {
  init_static_variables();
  Process::InitProcessStatics();
  PageletServer::Restart();
  XboxServer::Restart();
  Extension::InitModules();
  apc_load(RuntimeOption::ApcLoadThread);
  StaticString::FinishInit();
  Eval::Debugger::StartServer();
}

void hphp_session_init() {
  ThreadInfo::s_threadInfo->onSessionInit();
  MemoryManager::TheMemoryManager()->resetStats();

  if (!s_warmup_state->done) {
    free_global_variables(); // just to be safe
    init_global_variables();
  }

#ifdef ENABLE_SIMPLE_COUNTER
  SimpleCounter::Enabled = true;
  StackTrace::Enabled = true;
#endif
}

bool hphp_is_warmup_enabled() {
  return s_warmup_state->enabled;
}

void hphp_set_warmup_enabled() {
  s_warmup_state->enabled = true;
}

ExecutionContext *hphp_context_init() {
  ExecutionContext *context = g_context.get();
  context->obStart();
  context->obProtect(true);
  return context;
}

static bool hphp_warmup(ExecutionContext *context,
                        const string &warmupDoc,
                        const string &reqInitFunc,
                        const string &reqInitDoc, bool &error) {
  bool ret = true;
  error = false;
  std::string errorMsg;
  if (!s_warmup_state->done) {
    MemoryManager *mm = MemoryManager::TheMemoryManager().get();
    if (mm->beforeCheckpoint()) {
      if (!s_warmup_state->failed) {
        s_warmup_state->enabled = true;
        if (!warmupDoc.empty()) {
          try {
            ServerStatsHelper ssh("warmup");
            include_impl_invoke(warmupDoc, true, get_variable_table());
          } catch (...) {
            ret = handle_exception(context, errorMsg, WarmupDocException,
                                   error);
          }
        }
      }
      if (!ret) {
        hphp_session_init();
        s_warmup_state->enabled = false;
        s_warmup_state->failed = true;
        return ret;
      }
      s_warmup_state->done = true;
      mm->checkpoint();
      s_warmup_state->atCheckpoint = true;
      context->backupSession();
    }
  }

  if (s_warmup_state->enabled && s_warmup_state->atCheckpoint) {
    ServerStatsHelper ssh("reqinit");
    try {
      if (!reqInitDoc.empty()) {
        include_impl_invoke(reqInitDoc, true, get_variable_table());
      }
      if (!reqInitFunc.empty()) {
        invoke(reqInitFunc.c_str(), Array());
      }
      context->backupSession();
    } catch (...) {
      ret = handle_exception(context, errorMsg, ReqInitException, error);
    }
  }

  s_warmup_state->atCheckpoint = false;
  return ret;
}

static void handle_invoke_exception(bool &ret, ExecutionContext *context,
                                    std::string &errorMsg, bool &error) {
  try {
    if (!handle_exception(context, errorMsg, InvokeException, error)) {
      ret = false;
    }
  } catch (...) {
    if (!handle_exception(context, errorMsg, HandlerException, error)) {
      ret = false;
    }
    context->obEndAll();
  }
}

bool hphp_invoke_simple(const std::string &filename) {
  bool error; string errorMsg;
  return hphp_invoke(g_context.get(), filename, false, null_array, null,
                     "", "", "", error, errorMsg);
}

bool hphp_invoke(ExecutionContext *context, const std::string &cmd,
                 bool func, CArrRef funcParams, Variant funcRet,
                 const string &warmupDoc, const string &reqInitFunc,
                 const string &reqInitDoc,
                 bool &error, string &errorMsg) {
  bool isServer = (strcmp(RuntimeOption::ExecutionMode, "srv") == 0);
  error = false;

  if (RuntimeOption::SandboxMode && !warmupDoc.empty()) {
    // Sandbox mode shouldn't do warmup, because
    //   (1) The checkpoint after warmup only records smart-allocated
    //       objects, not things like ClassInfo.
    //   (2) The php files under sandbox mode is subject to frequent change,
    //       which might invalidate the warmed-up state.
    Logger::Warning("WarmupDocument is ignored under the sandbox mode; "
                    "use RequestInitDocument instead.");
  }

  String oldCwd;
  if (isServer) {
    oldCwd = context->getCwd();
    if (!warmupDoc.empty() && !RuntimeOption::SandboxMode) {
      hphp_chdir_file(warmupDoc);
    }
  }
  if (!hphp_warmup(context, RuntimeOption::SandboxMode ? "" : warmupDoc,
                   reqInitFunc, reqInitDoc, error)) {
    if (isServer) context->setCwd(oldCwd);
    return false;
  }

  bool ret = true;
  try {
    ServerStatsHelper ssh("invoke");
    if (func) {
      funcRet = invoke(cmd.c_str(), funcParams);
    } else {
      if (isServer) hphp_chdir_file(cmd);
      include_impl_invoke(cmd.c_str(), true, get_variable_table());
    }
  } catch (...) {
    handle_invoke_exception(ret, context, errorMsg, error);
  }

  try {
    context->onShutdownPreSend();
  } catch (...) {
    handle_invoke_exception(ret, context, errorMsg, error);
  }

  if (isServer) context->setCwd(oldCwd);
  return ret;
}

void hphp_context_exit(ExecutionContext *context, bool psp,
                       bool shutdown /* = true */,
                       const char *program /* = NULL */) {
  if (psp) {
    context->onShutdownPostSend();
  }
  if (RuntimeOption::EnableDebugger) {
    try {
      Eval::Debugger::InterruptPSPEnded(program);
    } catch (const Eval::DebuggerException &e) {}
  }
  Eval::RequestEvalState::DestructObjects();
  if (shutdown) {
    context->onRequestShutdown();
  }
  context->obProtect(false);
  context->obEndAll();
}

void hphp_session_exit() {
  FiberAsyncFunc::OnRequestExit();
  Eval::RequestEvalState::Reset();
  // Server note has to live long enough for the access log to fire.
  // RequestLocal is too early.
  ServerNote::Reset();
  g_context.reset();

  MemoryManager *mm = MemoryManager::TheMemoryManager().get();
  if (RuntimeOption::CheckMemory) {
    mm->checkMemory(false);
  }
  if (RuntimeOption::EnableStats && RuntimeOption::EnableMemoryStats) {
    mm->logStats();
  }
  mm->resetStats();

  if (mm->afterCheckpoint()) {
    ServerStatsHelper ssh("rollback");
    mm->sweepAll();

    /**
     * We have to do it again, because sweep() may call g_context-> or
     * RequestLocal<T>, which also calls g_context->, to create a new
     * ExecutionContext object that has SmartAllocated data members. These
     * members cannot survive over rollback(), so we need to delete g_context.
     */
    g_context.reset();

    mm->rollback();
    s_warmup_state->atCheckpoint = true;
  } else {
    ServerStatsHelper ssh("free");
    free_global_variables();
  }

  ThreadInfo::s_threadInfo->onSessionExit();
}

void hphp_process_exit() {
  Eval::Debugger::Stop();
  Extension::ShutdownModules();
}

///////////////////////////////////////////////////////////////////////////////
}
