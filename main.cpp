#include "opencc.h"
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <unistd.h>
#include <vector>

typedef enum : uint8_t {
  FILE_NONE, FILE_C, FILE_ASM, FILE_OBJ, FILE_AR, FILE_DSO,
} FileType;




StringArray include_paths;
bool opt_fcommon = true;
bool opt_fpic;

static FileType opt_x;
static StringArray opt_include;
static bool opt_E;
static bool opt_M;
static bool opt_MD;
static bool opt_MMD;
static bool opt_MP;
static bool opt_S;
static bool opt_c;
static bool opt_cc1;
static bool opt_hash_hash_hash;
static bool opt_static;
static bool opt_shared;
static std::string opt_MF;
static std::string opt_MT;
static std::string opt_o;

static StringArray ld_extra_args;
static StringArray std_include_paths;

std::string base_file;
static std::string output_file;

static StringArray input_paths;
static StringArray tmpfiles;

static void usage(int status) {
  fprintf(stderr, "chibicc [ -o <path> ] <file>\n");
  exit(status);
}

static bool take_arg(std::string arg) {
  const std::string x[] = {
    "-o", "-I", "-idirafter", "-include", "-x", "-MF", "-MT", "-Xlinker",
  };

  for (int i = 0; i < sizeof(x) / sizeof(*x); i++)
    if ((arg == x[i]))
      return true;
  return false;
}

static void add_default_include_paths(std::string argv0) {
  // We expect that chibicc-specific include files are installed
  // to ./include relative to argv[0].
  strarray_push(include_paths, format("%s/include", dirname(strdup(argv0.c_str()))));

  // Add standard include paths.
  strarray_push(include_paths, "/usr/local/include");
  strarray_push(include_paths, "/usr/include/x86_64-linux-gnu");
  strarray_push(include_paths, "/usr/include");

  // Keep a copy of the standard include paths for -MMD option.
  for (int i = 0; i < include_paths.size(); i++)
    strarray_push(std_include_paths, include_paths.at(i).c_str());
}

static void define(std::string str) {
  const char *eq = strchr(str.c_str(), '=');
  if (eq)
    define_macro(strndup(str.data(), eq - str.data()), const_cast<char*>(eq + 1));
  else
    define_macro(str, "1");
}

static FileType parse_opt_x(std::string s) {
  if (s == "c")
    return FILE_C;
  if (s =="assembler")
    return FILE_ASM;
  if (s == "none")
    return FILE_NONE;
  error(format("<command line>: unknown argument for -x: %s", s.c_str()));
}

static std::string quote_makefile(std::string s) {
  std::string buf; 
  buf.resize(s.size() * 2 + 1);

  for (int i = 0, j = 0; s[i]; i++) {
    switch (s[i]) {
    case '$':
      buf[j++] = '$';
      buf[j++] = '$';
      break;
    case '#':
      buf[j++] = '\\';
      buf[j++] = '#';
      break;
    case ' ':
    case '\t':
      for (int k = i - 1; k >= 0 && s[k] == '\\'; k--)
        buf[j++] = '\\';
      buf[j++] = '\\';
      buf[j++] = s[i];
      break;
    default:
      buf[j++] = s[i];
      break;
    }
  }
  return buf;
}

static void parse_args(int argc, char** argv) {
  // Make sure that all command line options that take an argument
  // have an argument.
  for (int i = 1; i < argc; i++)
    if (take_arg(argv[i]))
      if (!argv[++i])
        usage(1);

  StringArray idirafter = {};

  for (int i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "-###")) {
      opt_hash_hash_hash = true;
      continue;
    }

    if (!strcmp(argv[i], "-cc1")) {
      opt_cc1 = true;
      continue;
    }

    if (!strcmp(argv[i], "--help"))
      usage(0);

    if (!strcmp(argv[i], "-o")) {
      opt_o = argv[++i];
      continue;
    }

    if (!strncmp(argv[i], "-o", 2)) {
      opt_o = argv[i] + 2;
      continue;
    }

    if (!strcmp(argv[i], "-S")) {
      opt_S = true;
      continue;
    }

    if (!strcmp(argv[i], "-fcommon")) {
      opt_fcommon = true;
      continue;
    }

    if (!strcmp(argv[i], "-fno-common")) {
      opt_fcommon = false;
      continue;
    }

    if (!strcmp(argv[i], "-c")) {
      opt_c = true;
      continue;
    }

    if (!strcmp(argv[i], "-E")) {
      opt_E = true;
      continue;
    }

    if (!strncmp(argv[i], "-I", 2)) {
      strarray_push(include_paths, argv[i] + 2);
      continue;
    }

    if (!strcmp(argv[i], "-D")) {
      define(argv[++i]);
      continue;
    }

    if (!strncmp(argv[i], "-D", 2)) {
      define(argv[i] + 2);
      continue;
    }

    if (!strcmp(argv[i], "-U")) {
      undef_macro(argv[++i]);
      continue;
    }

    if (!strncmp(argv[i], "-U", 2)) {
      undef_macro(argv[i] + 2);
      continue;
    }

    if (!strcmp(argv[i], "-include")) {
      strarray_push(opt_include, argv[++i]);
      continue;
    }

    if (!strcmp(argv[i], "-x")) {
      opt_x = parse_opt_x(argv[++i]);
      continue;
    }

    if (!strncmp(argv[i], "-x", 2)) {
      opt_x = parse_opt_x(argv[i] + 2);
      continue;
    }

    if (!strncmp(argv[i], "-l", 2) || !strncmp(argv[i], "-Wl,", 4)) {
      strarray_push(input_paths, argv[i]);
      continue;
    }

    if (!strcmp(argv[i], "-Xlinker")) {
      strarray_push(ld_extra_args, argv[++i]);
      continue;
    }

    if (!strcmp(argv[i], "-s")) {
      strarray_push(ld_extra_args, "-s");
      continue;
    }

    if (!strcmp(argv[i], "-M")) {
      opt_M = true;
      continue;
    }

    if (!strcmp(argv[i], "-MF")) {
      opt_MF = argv[++i];
      continue;
    }

    if (!strcmp(argv[i], "-MP")) {
      opt_MP = true;
      continue;
    }

    if (!strcmp(argv[i], "-MT")) {
      if (opt_MT.empty())
        opt_MT = argv[++i];
      else
        opt_MT = format("%s %s", opt_MT.c_str(), argv[++i]);
      continue;
    }

    if (!strcmp(argv[i], "-MD")) {
      opt_MD = true;
      continue;
    }

    if (!strcmp(argv[i], "-MQ")) {
      if (opt_MT.empty())
        opt_MT = quote_makefile(argv[++i]);
      else {
        std::string argv_i = argv[++i];
        opt_MT = format("%s %s", opt_MT.c_str(), quote_makefile(argv_i).c_str());
      }
      continue;
    }

    if (!strcmp(argv[i], "-MMD")) {
      opt_MD = opt_MMD = true;
      continue;
    }

    if (!strcmp(argv[i], "-fpic") || !strcmp(argv[i], "-fPIC")) {
      opt_fpic = true;
      continue;
    }

    if (!strcmp(argv[i], "-cc1-input")) {
      base_file = argv[++i];
      continue;
    }

    if (!strcmp(argv[i], "-cc1-output")) {
      output_file = argv[++i];
      continue;
    }

    if (!strcmp(argv[i], "-idirafter")) {
      strarray_push(idirafter, argv[++i]);
      continue;
    }

    if (!strcmp(argv[i], "-static")) {
      opt_static = true;
      strarray_push(ld_extra_args, "-static");
      continue;
    }

    if (!strcmp(argv[i], "-shared")) {
      opt_shared = true;
      strarray_push(ld_extra_args, "-shared");
      continue;
    }

    if (!strcmp(argv[i], "-L")) {
      strarray_push(ld_extra_args, "-L");
      strarray_push(ld_extra_args, argv[++i]);
      continue;
    }

    if (!strncmp(argv[i], "-L", 2)) {
      strarray_push(ld_extra_args, "-L");
      strarray_push(ld_extra_args, argv[i] + 2);
      continue;
    }

    if (!strcmp(argv[i], "-hashmap-test")) {
      hashmap_test();
      exit(0);
    }

    // These options are ignored for now.
    if (!strncmp(argv[i], "-O", 2) ||
        !strncmp(argv[i], "-W", 2) ||
        !strncmp(argv[i], "-g", 2) ||
        !strncmp(argv[i], "-std=", 5) ||
        !strcmp(argv[i], "-ffreestanding") ||
        !strcmp(argv[i], "-fno-builtin") ||
        !strcmp(argv[i], "-fno-omit-frame-pointer") ||
        !strcmp(argv[i], "-fno-stack-protector") ||
        !strcmp(argv[i], "-fno-strict-aliasing") ||
        !strcmp(argv[i], "-m64") ||
        !strcmp(argv[i], "-mno-red-zone") ||
        !strcmp(argv[i], "-w"))
      continue;

    if (argv[i][0] == '-' && argv[i][1] != '\0')
      error(format("unknown argument: %s", argv[i]));

    strarray_push(input_paths, argv[i]);
  }

  for (int i = 0; i < idirafter.size(); i++)
    strarray_push(include_paths, idirafter.at(i).c_str());

  if (!opt_cc1 && input_paths.size() == 0)
    error("no input files");

  // -E implies that the input is the C macro language.
  if (opt_E)
    opt_x = FILE_C;
}

static FILE *open_file(const std::string path) {
  if (path.empty() || path == "-")
    return stdout;

  FILE *out = fopen(path.c_str(), "w");
  if (!out)
    error(format("cannot open output file: %s: %s", path.c_str(), strerror(errno)));
  return out;
}

inline bool endswith(const std::string& p, const std::string& q) {
  return p.ends_with(q);
}

// Replace file extension
static std::string replace_extn(std::string tmpl, std::string extn) {
  std::string filename = basename(strdup(tmpl.c_str()));
  auto dot = filename.rfind('.');
  if (dot != std::string::npos)
    filename.resize(dot);
  return format("%s%s", filename.c_str(), extn.c_str());
}

static void cleanup() {
  for (int i = 0; i < tmpfiles.size(); i++)
    unlink(tmpfiles.at(i).c_str());
}

static std::string create_tmpfile() {
  std::string path = strdup("/tmp/chibicc-XXXXXX");
  int fd = mkstemp(path.data());
  if (fd == -1)
    error(format("mkstemp failed: %s", strerror(errno)));
  close(fd);

  strarray_push(tmpfiles, path);
  return path;
}

static void run_subprocess(char** argv) {
  // If -### is given, dump the subprocess's command line.
  if (opt_hash_hash_hash) {
    fprintf(stderr, "%s", argv[0]);
    for (int i = 1; argv[i]; i++)
      fprintf(stderr, " %s", argv[i]);
    fprintf(stderr, "\n");
  }

  pid_t pid = fork();
  if (pid < 0) {
    perror("fork");
    exit(1);
  }

  if (pid == 0) {
    // Child process. Run a new command.
    execvp(argv[0], argv);
    fprintf(stderr, "exec failed: %s: %s\n", argv[0], strerror(errno));
    arena.reset();
    _exit(1);
  }

  int status;
  if (waitpid(pid, &status, 0) == -1) {
    perror("waitpid");
    exit(1);
  }

  if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
    arena.reset();
    exit(1);
  }
}

static void run_cc1(int argc, char** argv, std::string input, std::string output) {
  std::vector<std::string> args;
  args.reserve(argc + 10);

  for (int i = 0; i < argc; i++)
    args.emplace_back(argv[i]);
  args.emplace_back("-cc1");

  if (!input.empty()) {
    args.emplace_back("-cc1-input");
    args.emplace_back(input);
  }

  if (!output.empty()) {
    args.emplace_back("-cc1-output");
    args.emplace_back(output);
  }

  std::vector<char*> cargs;
  cargs.reserve(args.size() + 1);
  for (auto& s : args)
    cargs.push_back(s.data());
  cargs.push_back(nullptr);

  run_subprocess(cargs.data());
}

// Print tokens to stdout. Used for -E.
static void print_tokens(Token *tok) {
  FILE *out = open_file(opt_o.c_str() ? opt_o.c_str() : "-");

  int line = 1;
  for (; tok->kind != TK_EOF; tok = tok->next) {
    if (line > 1 && tok->at_bol)
      fprintf(out, "\n");
    if (tok->has_space && !tok->at_bol)
      fprintf(out, " ");
    fprintf(out, "%.*s", tok->len, tok->loc.c_str());
    line++;
  }
  fprintf(out, "\n");
}

static bool in_std_include_path(std::string path) {
  for (int i = 0; i < std_include_paths.size(); i++) {
    const std::string dir = std_include_paths.at(i).c_str();
    int len = dir.size();
    if (strncmp(dir.c_str(), path.c_str(), len) == 0 && path[len] == '/')
      return true;
  }
  return false;
}

// If -M options is given, the compiler write a list of input files to
// stdout in a format that "make" command can read. This feature is
// used to automate file dependency management.
static void print_dependencies() {
  std::string path;
  if (!opt_MF.empty())
    path = opt_MF;
  else if (opt_MD) {
    std::string dir = dirname(strdup(base_file.c_str()));
    std::string name = replace_extn(base_file.c_str(), ".d");
    path = (dir == ".") ? name : dir + "/" + name;
  }
  else if (!opt_o.empty())
    path = opt_o;
  else
    path = "-";

  FILE *out = open_file(path);
  if (!opt_MT.empty())
    fprintf(out, "%s:", opt_MT.c_str());
  else
    fprintf(out, "%s:", quote_makefile(replace_extn(base_file.c_str(), ".o")).c_str());

  File **files = get_input_files();

  for (int i = 0; files[i]; i++) {
    if (opt_MMD && in_std_include_path(files[i]->name))
      continue;
    fprintf(out, " \\\n  %s", files[i]->name.c_str());
  }

  fprintf(out, "\n\n");

  if (opt_MP) {
    for (int i = 1; files[i]; i++) {
      if (opt_MMD && in_std_include_path(files[i]->name))
        continue;
      fprintf(out, "%s:\n\n", quote_makefile(files[i]->name).c_str());
    }
  }
}

static Token *must_tokenize_file(std::string path) {
  Token *tok = tokenize_file(path);
  if (!tok)
    error(format("%s: %s", path.c_str(), strerror(errno)));
  return tok;
}

static Token *append_tokens(Token *tok1, Token *tok2) {
  if (!tok1 || tok1->kind == TK_EOF)
    return tok2;

  Token *t = tok1;
  while (t->next->kind != TK_EOF)
    t = t->next;
  t->next = tok2;
  return tok1;
}

static void cc1() {
  strarray_push(include_paths, dirname(strdup(base_file.c_str())));
  Token *tok = nullptr;

  // Process -include option
  for (int i = 0; i < opt_include.size(); i++) {
    std::string incl = opt_include.at(i).c_str();

    std::string path;
    if (file_exists(incl.data())) {
      path = incl;
    } else {
      path = search_include_paths(incl.data());
      if (path.empty())
        error(format("-include: %s: %s", incl.data(), strerror(errno)));
    }

    Token *tok2 = must_tokenize_file(path.data());
    tok = append_tokens(tok, tok2);
  }

  // Tokenize and parse.
  Token *tok2 = must_tokenize_file(base_file);
  tok = append_tokens(tok, tok2);
  tok = preprocess(tok);

  // If -M or -MD are given, print file dependencies.
  if (opt_M || opt_MD) {
    print_dependencies();
    if (opt_M)
      return;
  }

  // If -E is given, print out preprocessed C code as a result.
  if (opt_E) {
    print_tokens(tok);
    return;
  }

  Obj *prog = parse(tok);

  // Open a temporary output buffer.
  char* buf;
  size_t buflen;
  FILE *output_buf = open_memstream(&buf, &buflen);

  // Traverse the AST to emit assembly.
  codegen(prog, output_buf);
  fclose(output_buf);

  // Write the asembly text to a file.
  FILE *out = open_file(output_file);
  fwrite(buf, buflen, 1, out);
  fclose(out);
}

static void assemble(std::string& input, std::string& output) {
  char* cmd[] = {
    const_cast<char*>("as"),
    const_cast<char*>("-c"),
    const_cast<char*>(input.c_str()),
    const_cast<char*>("-o"),
    const_cast<char*>(output.c_str()),
    nullptr
  };
  run_subprocess(cmd);
}

static std::string find_file(std::string pattern) {
  std::string path;
  glob_t buf = {};
  glob(pattern.c_str(), 0, nullptr, &buf);
  if (buf.gl_pathc > 0)
    path = strdup(buf.gl_pathv[buf.gl_pathc - 1]);
  globfree(&buf);
  return path;
}

// Returns true if a given file exists.
bool file_exists(std::string path) {
  struct stat st;
  return !stat(path.c_str(), &st);
}

static std::string find_libpath() {
  if (file_exists("/usr/lib/x86_64-linux-gnu/crti.o"))
    return "/usr/lib/x86_64-linux-gnu";
  if (file_exists("/usr/lib64/crti.o"))
    return "/usr/lib64";
  error("library path is not found");
}

static std::string find_gcc_libpath() {
  std::string paths[] = {
    "/usr/lib/gcc/x86_64-linux-gnu/*/crtbegin.o",
    "/usr/lib/gcc/x86_64-pc-linux-gnu/*/crtbegin.o", // For Gentoo
    "/usr/lib/gcc/x86_64-redhat-linux/*/crtbegin.o", // For Fedora
  };

  for (int i = 0; i < sizeof(paths) / sizeof(*paths); i++) {
    std::string path = find_file(paths[i]);
    if (!path.empty())
      return dirname(path.data());
  }

  error("gcc library path is not found");
}

static void run_linker(StringArray& inputs, const std::string output) {
  StringArray arr = {};

  strarray_push(arr, "ld");
  strarray_push(arr, "-o");
  strarray_push(arr, output);
  strarray_push(arr, "-m");
  strarray_push(arr, "elf_x86_64");

  std::string libpath = find_libpath();
  std::string gcc_libpath = find_gcc_libpath();

  if (opt_shared) {
    strarray_push(arr, format("%s/crti.o", libpath.c_str()));
    strarray_push(arr, format("%s/crtbeginS.o", gcc_libpath.c_str()));
  } else {
    strarray_push(arr, format("%s/crt1.o", libpath.c_str()));
    strarray_push(arr, format("%s/crti.o", libpath.c_str()));
    strarray_push(arr, format("%s/crtbegin.o", gcc_libpath.c_str()));
  }

  strarray_push(arr, format("-L%s", gcc_libpath.c_str()));
  strarray_push(arr, "-L/usr/lib/x86_64-linux-gnu");
  strarray_push(arr, "-L/usr/lib64");
  strarray_push(arr, "-L/lib64");
  strarray_push(arr, "-L/usr/lib/x86_64-linux-gnu");
  strarray_push(arr, "-L/usr/lib/x86_64-pc-linux-gnu");
  strarray_push(arr, "-L/usr/lib/x86_64-redhat-linux");
  strarray_push(arr, "-L/usr/lib");
  strarray_push(arr, "-L/lib");

  if (!opt_static) {
    strarray_push(arr, "-dynamic-linker");
    strarray_push(arr, "/lib64/ld-linux-x86-64.so.2");
  }

  for (int i = 0; i < ld_extra_args.size(); i++)
    strarray_push(arr, ld_extra_args.at(i).c_str());

  for (int i = 0; i < inputs.size(); i++)
    strarray_push(arr, inputs.at(i).c_str());

  if (opt_static) {
    strarray_push(arr, "--start-group");
    strarray_push(arr, "-lgcc");
    strarray_push(arr, "-lgcc_eh");
    strarray_push(arr, "-lc");
    strarray_push(arr, "--end-group");
  } else {
    strarray_push(arr, "-lc");
    strarray_push(arr, "-lgcc");
    strarray_push(arr, "--as-needed");
    strarray_push(arr, "-lgcc_s");
    strarray_push(arr, "--no-as-needed");
  }

  if (opt_shared)
    strarray_push(arr, format("%s/crtendS.o", gcc_libpath.c_str()));
  else
    strarray_push(arr, format("%s/crtend.o", gcc_libpath.c_str()));

  strarray_push(arr, format("%s/crtn.o", libpath.c_str()));

  std::vector<char*> cargs;
  for (auto& s : arr)
      cargs.push_back(s.data());

  cargs.push_back(nullptr);  
  char** args = cargs.data(); 
  run_subprocess(args);
}

static FileType get_file_type(std::string filename) {
  if (opt_x != FILE_NONE)
    return opt_x;

  if (endswith(filename, ".a"))
    return FILE_AR;
  if (endswith(filename, ".so"))
    return FILE_DSO;
  if (endswith(filename, ".o"))
    return FILE_OBJ;
  if (endswith(filename, ".c"))
    return FILE_C;
  if (endswith(filename, ".s"))
    return FILE_ASM;

  error(format("<command line>: unknown file extension: %s", filename.c_str()));
}

int main(int argc, char** argv) {
  atexit(cleanup);
  init_macros();
  parse_args(argc, argv);

  if (opt_cc1) {
    add_default_include_paths(argv[0]);
    cc1();
    return 0;
  }

  if (input_paths.size() > 1 && !opt_o.empty() && (opt_c || opt_S || opt_E))
    error("cannot specify '-o' with '-c,' '-S' or '-E' with multiple files");

  StringArray ld_args = {};

  for (int i = 0; i < input_paths.size(); i++) {
    std::string input = input_paths.at(i).c_str();

    if (input.compare(0, 2, "-l")==0) {
      strarray_push(ld_args, input.data());
      continue;
    }

    if (input.compare(0, 4, "-Wl")==0) {
      std::string s = strdup(input.data() + 4);
      char* arg = strtok(s.data(), ",");
      while (arg) {
        strarray_push(ld_args, arg);
        arg = strtok(nullptr, ",");
      }
      continue;
    }

    std::string output;
    if (!opt_o.empty())
      output = opt_o;
    else if (opt_S)
      output = replace_extn(input.data(), ".s");
    else
      output = replace_extn(input.data(), ".o");

    FileType type = get_file_type(input.data());

    // Handle .o or .a
    if (type == FILE_OBJ || type == FILE_AR || type == FILE_DSO) {
      strarray_push(ld_args, input.data());
      continue;
    }

    // Handle .s
    if (type == FILE_ASM) {
      if (!opt_S)
        assemble(input, output);
      continue;
    }

    assert(type == FILE_C);

    // Just preprocess
    if (opt_E || opt_M) {
      run_cc1(argc, argv, input.data(), "");
      continue;
    }

    // Compile
    if (opt_S) {
      run_cc1(argc, argv, input.data(), output);
      continue;
    }

    // Compile and assemble
    if (opt_c) {
      std::string tmp = create_tmpfile();
      run_cc1(argc, argv, input.data(), tmp);
      assemble(tmp, output);
      continue;
    }

    // Compile, assemble and link
    std::string tmp1 = create_tmpfile();
    std::string tmp2 = create_tmpfile();
    run_cc1(argc, argv, input.data(), tmp1);
    assemble(tmp1, tmp2);
    strarray_push(ld_args, tmp2);
    continue;
  }

  if (ld_args.size() > 0)
    run_linker(ld_args, !opt_o.empty() ? opt_o : "a.out");
  return 0;
}
