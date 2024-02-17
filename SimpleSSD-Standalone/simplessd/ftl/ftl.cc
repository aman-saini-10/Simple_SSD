/*
 * Copyright (C) 2017 CAMELab
 *
 * This file is part of SimpleSSD.
 *
 * SimpleSSD is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * SimpleSSD is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with SimpleSSD.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "ftl/ftl.hh"

#include "ftl/page_mapping.hh"
#include "icl/global_point.hh"

namespace SimpleSSD {

namespace FTL {

FTL::FTL(ConfigReader &c, DRAM::AbstractDRAM *d) : conf(c), pDRAM(d) {
  PAL::Parameter *palparam;

  pPAL = new PAL::PAL(conf);
  palparam = pPAL->getInfo();

  param.totalPhysicalBlocks = palparam->superBlock;
  param.totalLogicalBlocks =
      palparam->superBlock *
      (1 - conf.readFloat(CONFIG_FTL, FTL_OVERPROVISION_RATIO));
  param.pagesInBlock = palparam->page;
  param.pageSize = palparam->superPageSize;
  param.ioUnitInPage = palparam->pageInSuperPage;
  printf("SuperPageSize %u\n",param.pageSize);
  param.pageCountToMaxPerf = palparam->superBlock / palparam->block;

  switch (conf.readInt(CONFIG_FTL, FTL_MAPPING_MODE)) {
    case PAGE_MAPPING:
      pFTL = new PageMapping(conf, param, pPAL, pDRAM);
      break;
  }

  if (param.totalPhysicalBlocks <=
      param.totalLogicalBlocks + param.pageCountToMaxPerf) {
    panic("FTL Over-Provision Ratio is too small");
  }

  // Print mapping Information
  printf("Total physical blocks %lu\n", param.totalPhysicalBlocks);
  printf("Total logical blocks %lu\n", param.totalLogicalBlocks);
  printf("Logical page size %u\n", param.pageSize);
   printf("Max prefect %u\n", param.pageCountToMaxPerf);

  // Initialize pFTL
  pFTL->initialize();
}

FTL::~FTL() {
  delete pPAL;
  delete pFTL;
}

void FTL::read(Request &req, uint64_t &tick) {
  debugprint(LOG_FTL, "READ  | LPN %" PRIu64, req.lpn);
//printf("In FTL the LPN %lu\n",req.lpn);
//LbaAccessFrequency[req.lpn]++;
  pFTL->read(req, tick);

  tick += applyLatency(CPU::FTL, CPU::READ);// this is the latency for accencing the flash
}
void FTL:: DecachePage(uint64_t lpn)
{
  pFTL->undoCached(lpn);
  //lpn=lpn+1;
}
void FTL::write(Request &req, uint64_t &tick) {
  debugprint(LOG_FTL, "WRITE | LPN %" PRIu64, req.lpn);
 // printf("in ftl Write req with %lu\n",req.lpn);
 
 //LbaAccessFrequency[req.lpn]++;
        
  pFTL->write(req, tick);
  //pFTL->getStatus(req.lpn,req.lpn);

  tick += applyLatency(CPU::FTL, CPU::WRITE);
}

void FTL::trim(Request &req, uint64_t &tick) {
  debugprint(LOG_FTL, "TRIM  | LPN %" PRIu64, req.lpn);

  pFTL->trim(req, tick);

  tick += applyLatency(CPU::FTL, CPU::TRIM);
}

void FTL::format(LPNRange &range, uint64_t &tick) {
  pFTL->format(range, tick);

  tick += applyLatency(CPU::FTL, CPU::FORMAT);
} 

Parameter *FTL::getInfo() {
  return &param;
}

uint64_t FTL::getUsedPageCount(uint64_t lpnBegin, uint64_t lpnEnd) {
  return pFTL->getStatus(lpnBegin, lpnEnd)->mappedLogicalPages;
}

void FTL::getStatList(std::vector<Stats> &list, std::string prefix) {
  pFTL->getStatList(list, prefix + "ftl.");
  pPAL->getStatList(list, prefix);
}

void FTL::getStatValues(std::vector<double> &values) {
  pFTL->getStatValues(values);
  pPAL->getStatValues(values);
  pFTL->printFtlStats();
}

void FTL::resetStatValues() {
  pFTL->resetStatValues();
  pPAL->resetStatValues();
}

}  // namespace FTL

}  // namespace SimpleSSD