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
#include "llvm/Object/Archive.h"
#include "llvm/Object/COFFImportFile.h"
#include "llvm/Object/ELFObjectFile.h"
#include "llvm/Object/MachOUniversal.h"
#include "llvm/Object/ObjectFile.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/FileSystem.h"
#include "llvm/Support/Format.h"
#include "llvm/Linker/Linker.h"
#include "llvm/Support/Path.h"
#include "llvm/Support/PrettyStackTrace.h"
#include "llvm/Support/ScopedPrinter.h"
#include "llvm/Support/Signals.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/IR/Module.h"
#include "llvm/LinkAllPasses.h"
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
static cl::opt<string> InputFilename("i", cl::desc("<input file>"),
                                     cl::Required);

void HandleMachOObjFile(MachOObjectFile *MachO) {
  if (MachO == nullptr) {
    report_fatal_error(make_error<GenericBinaryError>("MachO is NULL!"), false);
  }
  /*
    __LLVM,(__bitcode,__cmdline,__asm) Clang
        __swift, __cmdline
  */
  SMDiagnostic diag;
  LLVMContext ctx;
  Module module("Noctilucence",ctx);
  stringstream ldargs;
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
            /*char* val=nullptr;
            xar_subdoc_prop_get(doc,const_cast<const char*>("architecture"),const_cast<const char**>(&val));
            ldargs<<"-arch "<<val<<" ";*/
            
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



  }

  ModulePass* obfPass=createObfuscationPass();
  obfPass->runOnModule(module);
  delete obfPass;


}
/*
  ELFs:
  .llvmbc .llvmcmd .llvmasm Clang
*/

void HandleUniversalMachO(MachOUniversalBinary *MachO) {
  for (auto obj : MachO->objects()) {
    auto Bin = &obj;
    if (auto BinaryOrErr = Bin->getAsObjectFile()) {
      MachOObjectFile &MachO = *BinaryOrErr.get();
      HandleMachOObjFile(&MachO);
    }
  }
}

int main(int argc, char const *argv[]) {
  StringRef ToolName = argv[0];
  sys::PrintStackTraceOnErrorSignal(ToolName);
  PrettyStackTraceProgram X(argc, argv);
  llvm_shutdown_obj Y;
  cl::ParseCommandLineOptions(argc, argv);
  auto BinaryOrErr = createBinary(InputFilename);
  if (BinaryOrErr) {
    auto &Binary = *BinaryOrErr;
    if (Binary.getBinary()->isMachOUniversalBinary()) {
      MachOUniversalBinary *MOUB =
          dyn_cast<MachOUniversalBinary>(Binary.getBinary());
      errs() << "Found Universal MachO With" << MOUB->getNumberOfObjects()
             << " Objects\n";
      HandleUniversalMachO(MOUB);
    } else if (Binary.getBinary()->isMachO()) {
      errs() << "Found Thin MachO\n";
      HandleMachOObjFile(dyn_cast<MachOObjectFile>(Binary.getBinary()));
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
