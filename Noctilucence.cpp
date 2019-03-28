/*
 *  LLVM Bitcode Recompiler
    Copyright (C) 2017 Zhang(https://github.com/Naville/)
    Exceptions:
        Anyone who has associated with ByteDance in anyway at any past, current,
        or future time point is prohibited from direct using this piece of software
        or create any derivative from it.


    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as published
    by the Free Software Foundation, either version 3 of the License, or
    any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.
    You should have received a copy of the GNU Affero General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "llvm/ADT/Triple.h"
#include "llvm/Analysis/TargetLibraryInfo.h"
#include "llvm/CodeGen/TargetPassConfig.h"
#include "llvm/CodeGen/MachineModuleInfo.h"
#include "llvm/CodeGen/CommandFlags.inc"
#include "llvm/Object/Archive.h"
#include "llvm/Object/COFFImportFile.h"
#include "llvm/Object/ELFObjectFile.h"
#include "llvm/Object/MachOUniversal.h"
#include "llvm/Object/ObjectFile.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Format.h"
#include "llvm/Support/Program.h"
#include "llvm/Support/TargetRegistry.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/PrettyStackTrace.h"
#include "llvm/Support/ScopedPrinter.h"
#include "llvm/Support/Signals.h"
#include "llvm/Linker/Linker.h"
#include "llvm/Target/TargetOptions.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/LegacyPassManager.h"
#include <algorithm>
#include <assert.h>
#include <memory>
#include <sstream>
#include <string>
#include <xar/xar.h>

using namespace llvm;
using namespace llvm::sys;
using namespace llvm::sys::fs;
using namespace llvm::object;
using namespace std;
static cl::opt<string> OutputFilename("o", cl::desc("<output file>"),
                                     cl::Required);
static cl::opt<string> InputFilename("i", cl::desc("<input file>"),
                                     cl::Required);

static cl::opt<string> LDPath("ldpath",cl::init("/usr/bin/ld"),cl::desc("<input file>"));
static cl::opt<string> SDKROOTPATH("sdkroot", cl::init("/Applications/Xcode.app/Contents/Developer/Platforms/iPhoneOS.platform/Developer/SDKs/iPhoneOS.sdk"),cl::desc("SDKROOT"));
string HandleMachOObjFile(MachOObjectFile *MachO,const char** argv) {
  if (MachO == nullptr) {
    report_fatal_error(make_error<GenericBinaryError>("MachO is NULL!"), false);
  }
  /*
    __LLVM,(__bitcode,__cmdline,__asm) Clang
        __swift, __cmdline
  */
  string ret="";
  SMDiagnostic diag;
  LLVMContext ctx;
  Module module("Noctilucence",ctx);
  vector<StringRef> ldargs;
  ldargs.push_back(argv[0]);
  for (section_iterator iter = MachO->section_begin(), E = MachO->section_end();
       iter != E; ++iter) {
    SectionRef section = *iter;
    StringRef sectionName;
    StringRef segmentName =
        MachO->getSectionFinalSegmentName(section.getRawDataRefImpl());
    MachO->getSectionName(section.getRawDataRefImpl(), sectionName);
    if (segmentName.equals("__LLVM") && sectionName.equals("__bundle")) {
      StringRef contents;
      section.getContents(contents);
      SmallString<128> TmpModel;
      path::system_temp_directory(true, TmpModel);
      path::append(TmpModel, "NoctilucenceTemporaryXAR%%%%%.xar");
      Expected<TempFile> tfOrError=TempFile::create(TmpModel);
      if(tfOrError){
        TempFile& tf=tfOrError.get();
        raw_fd_ostream rf(tf.FD,false);
        rf<<contents;
        rf.flush();
        errs()<<"Created xar temporary file at:"<<tf.TmpName<<"\n";
        xar_t xar=xar_open(tf.TmpName.c_str(),READ);
        xar_iter_t xi=xar_iter_new();
        for ( xar_file_t xf = xar_file_first(xar, xi); xf; xf = xar_file_next(xi)) {
          char *buffer=nullptr;
          xar_extract_tobuffer(xar,xf,&buffer);
          assert(buffer!=nullptr && "xar extraction failed");
          char* sizeStr=xar_get_size(xar,xf);
          long int total=atol(sizeStr);
          free(sizeStr);

          MemoryBuffer* MB=MemoryBuffer::getMemBuffer(StringRef(buffer,total),"",false).get();
          MemoryBufferRef MBR(*MB);
          std::unique_ptr<Module> tmpModule=parseIR(MBR,diag,ctx);
          assert(tmpModule.get()!=nullptr && "Bitcode Parsing Failed!");
          Linker::linkModules(module,std::move(tmpModule));
          free(buffer);
        }

        for(xar_subdoc_t doc=xar_subdoc_first(xar);doc;doc = xar_subdoc_next(doc)){
          if(strcmp("Ld",xar_subdoc_name(doc))==0){
            unsigned char* val=nullptr;
            unsigned int size=0;
            xar_subdoc_copyout(doc,&val,&size);
            string text((char*)val);
            std::stringstream ss(text);
            std::string to;
            vector<char*> dylibs;
            while(std::getline(ss,to,'\n')){
              if(to.find("<option>")!=string::npos && to.find("</option>")!=string::npos){
                  size_t s1 = to.find("<option>");
                  to.erase(s1,strlen("<option>"));
                  size_t s2 = to.find("</option>");
                  to.erase(s2,strlen("</option>"));
                  to.erase(std::remove(to.begin(),to.end(), ' '),to.end());
                  char* newT=(char*)calloc(to.size()+1,sizeof(char));
                  to.copy(newT,to.size());
                  ldargs.push_back(newT);
              }
              else if(to.find("<architecture>")!=string::npos && to.find("</architecture>")!=string::npos){
                size_t s1 = to.find("<architecture>");
                to.erase(s1,strlen("<architecture>"));
                size_t s2 = to.find("</architecture>");
                to.erase(s2,strlen("</architecture>"));
                to.erase(std::remove(to.begin(),to.end(), ' '),to.end());
                ldargs.push_back("-arch");
                char* newT=(char*)calloc(to.size()+1,sizeof(char));
                to.copy(newT,to.size());
                ldargs.push_back(newT);
              }
              else if(to.find("<lib>")!=string::npos && to.find("</lib>")!=string::npos){
                  size_t s1 = to.find("<lib>");
                  to.erase(s1,strlen("<lib>"));
                  size_t s2 = to.find("</lib>");
                  to.erase(s2,strlen("</lib>"));
                  to.erase(std::remove(to.begin(),to.end(), ' '),to.end());
                  to.replace(to.find("{SDKPATH}"),strlen("{SDKPATH}"),SDKROOTPATH);
                  char* newT=(char*)calloc(to.size()+1,sizeof(char));
                  to.copy(newT,to.size());
                  dylibs.push_back(newT);
              }
            }
            for(char* dy:dylibs){
              ldargs.push_back(dy);
            }
            break;
          }
        }

        xar_close(xar);
        Error err=tf.discard();
        if(err){
          errs()<<err<<"\n";
          abort();
        }
      }
      else{
        errs()<<"Creating xar temporary file failed:"<<tfOrError.takeError()<<"\n";
        abort();
      }
      break;
    }
    else if(sectionName.equals("__cmdline")){
      errs()<<"Only Apple Embedded Bitcode is implemented!\n";
      abort();
    }



  }
  //TargetOptions's setup is by no mean complete, though
  SmallString<128> TmpModel;
  path::system_temp_directory(true, TmpModel);
  path::append(TmpModel, "NoctilucenceTemporaryObject%%%%%%%.o");
  Expected<TempFile> tfOrError=TempFile::create(TmpModel);
  if(tfOrError){
    TempFile& tf=tfOrError.get();
    errs()<<"Created temporary object file at:"<<tf.TmpName<<" with FD:"<<tf.FD<<"\n";
    raw_fd_ostream rf(tf.FD,false);
    Triple tri(module.getTargetTriple());
    DataLayout dl(&module);
    PassManagerBuilder PMB;
    legacy::FunctionPassManager FPM(&module);
    legacy::PassManager MPM;
    PMB.populateFunctionPassManager(FPM);
    PMB.populateModulePassManager(MPM);
    for (auto &F : module){
      FPM.run(F);
    }
    MPM.run(module);
    std::string err;
    string arch="";
    const Target *target = TargetRegistry::lookupTarget(tri.getTriple(),err);
    if(target==nullptr){
      errs()<<err<<"\n";
      abort();
    }
    TargetOptions Options = InitTargetOptionsFromCodeGenFlags();
    Options.DisableIntegratedAS = false;
    Options.MCOptions.ShowMCEncoding = false;
    Options.MCOptions.MCUseDwarfDirectory = false;
    Options.MCOptions.AsmVerbose = false;
    Options.MCOptions.PreserveAsmComments = false;
    Options.MCOptions.SplitDwarfFile = false;
    std::unique_ptr<TargetMachine> Target(target->createTargetMachine(
    tri.getTriple(),"","", Options, getRelocModel(),
    getCodeModel(), CodeGenOpt::Default));
    assert(Target && "Could not allocate target machine!");
    legacy::PassManager PM;
    TargetLibraryInfoImpl TLII(tri);
    PM.add(new TargetLibraryInfoWrapperPass(TLII));
    LLVMTargetMachine &LLVMTM = static_cast<LLVMTargetMachine&>(*Target);
    MachineModuleInfo *MMI = new MachineModuleInfo(&LLVMTM);
    TargetPassConfig &TPC = *LLVMTM.createPassConfig(PM);
    PM.add(&TPC);
    PM.add(MMI);
    Target->addPassesToEmitFile(PM,rf,nullptr,TargetMachine::CodeGenFileType::CGFT_ObjectFile,false,nullptr);
    PM.run(module);
    rf.flush();
    ldargs.push_back(tf.TmpName);
    //Prepare linking final obj

    SmallString<128> TmpModel2;
    path::system_temp_directory(true, TmpModel2);
    path::append(TmpModel2, "NoctilucenceFinalObject%%%%%%%");
    Expected<TempFile> tfOrError2=TempFile::create(TmpModel2);
    if(tfOrError2){
      TempFile& tf2=tfOrError2.get();
      errs()<<"Emitting Final Linked Product at:"<<tf2.TmpName<<"\n";
      ret=tf2.TmpName;
      ldargs.push_back("-o");
      ldargs.push_back(tf2.TmpName);
      ldargs.push_back("-syslibroot");
      ldargs.push_back(SDKROOTPATH);
      std::string ErrMsg;
      bool failed=false;
      sys::ExecuteAndWait(LDPath,ArrayRef<StringRef>(ldargs),llvm::None,{},0,0,&ErrMsg,&failed);
      if(failed || ErrMsg!=""){
        errs()<<"Linking Failed:"<<ErrMsg<<"\n";
        abort();
      }
      Error errc=tf2.keep();
      if(errc){
        errs()<<errc<<"\n";
        abort();
      }
    }
    else{
      errs()<<"Creating Linking Product Failed:"<<tfOrError2.takeError()<<"\n";
      abort();
    }
    Error errc=tf.discard();
    if(errc){
      errs()<<errc<<"\n";
      abort();
    }


  }
  else{
    errs()<<"Emit Object File Failed:"<<tfOrError.takeError()<<"\n";
  }
  return ret;

}
/*
  ELFs:
  .llvmbc .llvmcmd .llvmasm Clang
*/

void HandleUniversalMachO(MachOUniversalBinary *MachO,const char** argv) {
  vector<StringRef> lipoargs;
  lipoargs.push_back(argv[0]);
  lipoargs.push_back("-create");
  for (auto obj : MachO->objects()) {
    auto Bin = &obj;
    if (auto BinaryOrErr = Bin->getAsObjectFile()) {
      MachOObjectFile &MachO = *BinaryOrErr.get();
      string ret=HandleMachOObjFile(&MachO,argv);
      if(ret==""){
        errs()<<"Slide Handling Failed\n";
        abort();
      }
      lipoargs.push_back(ret);
    }
  }
  lipoargs.push_back("-output");
  lipoargs.push_back(OutputFilename);
  std::string ErrMsg;
  bool failed=false;
  sys::ExecuteAndWait("/usr/bin/lipo",ArrayRef<StringRef>(lipoargs),llvm::None,{},0,0,&ErrMsg,&failed);
  if(failed || ErrMsg!=""){
    errs()<<"Linking Failed:"<<ErrMsg<<"\n";
    abort();
  }
}

int main(int argc, char const *argv[]) {
  StringRef ToolName = argv[0];
  sys::PrintStackTraceOnErrorSignal(ToolName);
  PrettyStackTraceProgram X(argc, argv);
  llvm_shutdown_obj Y;
  cl::ParseCommandLineOptions(argc, argv);
  cl::AddExtraVersionPrinter(TargetRegistry::printRegisteredTargetsForVersion);
  InitializeAllTargets();
  InitializeAllTargetMCs();
  InitializeAllAsmPrinters();
  InitializeAllAsmParsers();
  auto BinaryOrErr = createBinary(InputFilename);
  if (BinaryOrErr) {
    auto &Binary = *BinaryOrErr;
    if (Binary.getBinary()->isMachOUniversalBinary()) {
      MachOUniversalBinary *MOUB =
          dyn_cast<MachOUniversalBinary>(Binary.getBinary());
      errs() << "Found Universal MachO With" << MOUB->getNumberOfObjects()
             << " Objects\n";
      HandleUniversalMachO(MOUB,argv);
    } else if (Binary.getBinary()->isMachO()) {
      errs() << "Found Thin MachO\n";
    string ret=HandleMachOObjFile(dyn_cast<MachOObjectFile>(Binary.getBinary()),argv);
    if(ret!=""){
      std::error_code err=sys::fs::rename(ret,OutputFilename);
      if(err){
        errs()<<"Moving failed with std::error_code :"<<err.message()<<"\n";
      }
    }
    else{
      errs()<<"Thin MachO Handling Failed\n";
    }

    } else {
      errs() << "Unsupported ObjectFile Format\n";
      return -1;
    }
    return 0;
  } else {
    errs() << "LLVM BinaryParsing Failed:"<<BinaryOrErr.takeError()<<"\n";
    return -1;
  }
}
