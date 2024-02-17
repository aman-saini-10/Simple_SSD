#include <cstdint>
#include <chrono>
#include <cstdlib>
#include <exception>
#include "ftl/page_mapping.hh"
#include <algorithm>
#include <limits>
#include <random>
#include <vector>
#include "ftl/config.hh"
#include "icl/abstract_cache.hh"
#include "icl/global_point.hh"
#include "icl/generic_cache.hh"
#include "util/algorithm.hh"
#include "util/bitset.hh"
#include "util/def.hh"
//#include "headers/global.hh"

namespace SimpleSSD {

namespace FTL {

PageMapping::PageMapping(ConfigReader &c, Parameter &p, PAL::PAL *l,
                         DRAM::AbstractDRAM *d)
    : AbstractFTL(p, l, d),
      pPAL(l),
      conf(c),
      lastFreeBlock(param.pageCountToMaxPerf),
      lastFreeBlockIOMap(param.ioUnitInPage),
      bReclaimMore(false),pageMoveStats(param.pagesInBlock){
  blocks.reserve(param.totalPhysicalBlocks);// number of blocks 
  table.reserve(param.totalLogicalBlocks * param.pagesInBlock);// overall table for mapping number of pages in the SSD.

  for (uint32_t i = 0; i < param.totalPhysicalBlocks; i++) {
    freeBlocks.emplace_back(Block(i, param.pagesInBlock, param.ioUnitInPage));// free block list with the defualt values
  }

  nFreeBlocks = param.totalPhysicalBlocks;

  status.totalLogicalPages = param.totalLogicalBlocks * param.pagesInBlock;
static const EVICT_POLICY policy =
      (EVICT_POLICY)conf.readInt(CONFIG_FTL, FTL_GC_EVICT_POLICY);
      if(policy==CACHED_GC)
      {
      printf("VictimBlockSelctionPolicy: CachedGC_ActualPaperCGCT\n");
      cachedGC=true;
      victimSelectionPolicy=4;
      }
      else if (policy==HOTBLOCK)
      {
           printf("VictimBlockSellctionPolicy: HotData\n");
        victimSelectionPolicy=5;
      }
      else
      {

             if(optimalreplacementPolicy){
              printf("VictimBlockSelctionPolicy: OptimalReplacement\n");
              }
              else
              {
                printf("VictimBlockSelctionPolicy: LruPolicy\n");
              }
              cachedGC=false;
              victimSelectionPolicy=0;;
              if(useDeadOnArrivalPredictor)
              {
                printf("DeadOnArrivalPredictior: Enabled\n");
              }
              else{
                printf("DeadOnArrivalPredictior: Disabled\n");
              }
              if(usePortionOfCacheAsShadow)
              {
                printf("CachePortionusedAsShadow\n");
              }

      }
  // Allocate free blocks
//printf("####Pages is in block##### %lu\n",param.pagesInBlock);

  for (uint32_t i = 0; i < param.pageCountToMaxPerf; i++) { // get free blocks equal to the maxPref
    lastFreeBlock.at(i) = getFreeBlock(i);
  }

  lastFreeBlockIndex = 0;
//pCache->write(Request &, uint64_t &);
  memset(&stat, 0, sizeof(stat));
    memset(&cmtStats, 0, sizeof(cmtStats));
  memset(&ftlStats, 0, sizeof(ftlStats));
 
//cmtSize= 262144;
//cmtSize = 131136 ; 65536
//cmtSize = 16392 ;
predictorThreshold = 2;
shadowwCacheSize = (5*cmtSize)/100; // shadowCache is 5% of cmt table;
if(usePortionOfCacheAsShadow)
{
cmtSize=cmtSize-shadowwCacheSize;
}
//cmtSize=8;
//optimalreplacementPolicy = true ;
  bRandomTweak = conf.readBoolean(CONFIG_FTL, FTL_USE_RANDOM_IO_TWEAK);
  bitsetSize = bRandomTweak ? param.ioUnitInPage : 1;
pFTL->getInfo();
 // pCache = new GenericCache(conf, pFTL, pDRAM);
 
}

PageMapping::~PageMapping() {}

bool PageMapping::initialize() {
  uint64_t nPagesToWarmup;
  uint64_t nPagesToInvalidate;
  uint64_t nTotalLogicalPages;
  uint64_t maxPagesBeforeGC;
  uint64_t tick;
  uint64_t valid;
  uint64_t invalid;
  FILLING_MODE mode;
startTime = std::chrono::high_resolution_clock::now();
  Request req(param.ioUnitInPage);

  debugprint(LOG_FTL_PAGE_MAPPING, "Initialization started");

  nTotalLogicalPages = param.totalLogicalBlocks * param.pagesInBlock;
  nPagesToWarmup =
      nTotalLogicalPages * conf.readFloat(CONFIG_FTL, FTL_FILL_RATIO);// percet of pages ued for warrmup
  nPagesToInvalidate =
      nTotalLogicalPages * conf.readFloat(CONFIG_FTL, FTL_INVALID_PAGE_RATIO);
  mode = (FILLING_MODE)conf.readUint(CONFIG_FTL, FTL_FILLING_MODE);
  maxPagesBeforeGC =
      param.pagesInBlock *
      (param.totalPhysicalBlocks *
           (1 - conf.readFloat(CONFIG_FTL, FTL_GC_THRESHOLD_RATIO)) -
       param.pageCountToMaxPerf);  // # free blocks to maintain

  if (nPagesToWarmup + nPagesToInvalidate > maxPagesBeforeGC) {
    warn("ftl: Too high filling ratio. Adjusting invalidPageRatio.");
    nPagesToInvalidate = maxPagesBeforeGC - nPagesToWarmup;;;
  }
printf("CMTSize: %lu\n",cmtSize);

printf("ShadowCacheSize: %lu\n",shadowwCacheSize);
printf("predictionThrushold: %lu\n",predictorThreshold);

printf("Total LogicalPages: %lu\n",nTotalLogicalPages);
printf("WarmUp Pages: %lu\n", nPagesToWarmup);
printf("PagesToInvalidate: %lu\n",nPagesToInvalidate);
printf("pages avaialbe for writes %lu\n",(nTotalLogicalPages-(nPagesToWarmup+nPagesToInvalidate)));

  debugprint(LOG_FTL_PAGE_MAPPING, "Total logical pages: %" PRIu64,
             nTotalLogicalPages);
  debugprint(LOG_FTL_PAGE_MAPPING,
             "Total logical pages to fill: %" PRIu64 " (%.2f %%)",
             nPagesToWarmup, nPagesToWarmup * 100.f / nTotalLogicalPages);
  debugprint(LOG_FTL_PAGE_MAPPING,
             "Total invalidated pages to create: %" PRIu64 " (%.2f %%)",
             nPagesToInvalidate,
             nPagesToInvalidate * 100.f / nTotalLogicalPages);

  req.ioFlag.set();
    printf("Ration of free blocks available Before Warmup %f nFreeBlocks %u Pages %lu\n",freeBlockRatio(),nFreeBlocks,nFreeBlocks*param.pagesInBlock);;
  // Step 1. Filling
  if (mode == FILLING_MODE_0 ||
      mode == FILLING_MODE_1) {  // for 0 and 1 it is sequentail fill
    // Sequential
    for (uint64_t i = 0; i < nPagesToWarmup; i++) {
      tick = 0;
      req.lpn = i;
      writeInternal(req, tick, false);
    }
  }
  else {
    // Random
    std::random_device rd;
   // std::mt19937_64 gen(rd());
   std::mt19937_64 gen(42);
    std::uniform_int_distribution<uint64_t> dist(0, nTotalLogicalPages - 1);

    for (uint64_t i = 0; i < nPagesToWarmup; i++) {
      tick = 0;
      req.lpn = dist(gen);// generate the logical addreses
      writeInternal(req, tick, false);// just fill the storage
    }
  }
  // printf("Number of page to invalidate %lu\n",nPagesToInvalidate);
  //  Step 2. Invalidating
  if (mode == FILLING_MODE_0) {
    // Sequential
    for (uint64_t i = 0; i < nPagesToInvalidate; i++) {
      tick = 0;
      req.lpn = i;
      writeInternal(req, tick, false);
    }
  }
  else if (mode == FILLING_MODE_1) {
    // Random
    // We can successfully restrict range of LPN to create exact number of
    // invalid pages because we wrote in sequential mannor in step 1.
    std::random_device rd;
    std::mt19937_64 gen(rd());
    std::uniform_int_distribution<uint64_t> dist(0, nPagesToWarmup - 1);

    for (uint64_t i = 0; i < nPagesToInvalidate; i++) {
      tick = 0;
      req.lpn = dist(gen);
     // writeInternal(req, tick, false);
     writeInternal(req, tick, false);

    }
  }
  else {
    // Random
    std::random_device rd;
    std::mt19937_64 gen(rd());
    std::uniform_int_distribution<uint64_t> dist(0, nTotalLogicalPages - 1);

    for (uint64_t i = 0; i < nPagesToInvalidate; i++) {
      tick = 0;
      req.lpn = dist(gen);
      writeInternal(req, tick, false);
    }
  }

  // Report
  calculateTotalPages(valid, invalid);  //
  memset(&ftlStats, 0, sizeof(ftlStats));

  printf("Valid Pages %lu Invalid Page %lu\n",valid,invalid);
  debugprint(LOG_FTL_PAGE_MAPPING, "Filling finished. Page status:");
  debugprint(LOG_FTL_PAGE_MAPPING,
             "  Total valid physical pages: %" PRIu64
             " (%.2f %%, target: %" PRIu64 ", error: %" PRId64 ")",
             valid, valid * 100.f / nTotalLogicalPages, nPagesToWarmup,
             (int64_t)(valid - nPagesToWarmup));
  debugprint(LOG_FTL_PAGE_MAPPING,
             "  Total invalid physical pages: %" PRIu64
             " (%.2f %%, target: %" PRIu64 ", error: %" PRId64 ")",
             invalid, invalid * 100.f / nTotalLogicalPages, nPagesToInvalidate,
             (int64_t)(invalid - nPagesToInvalidate));
  debugprint(LOG_FTL_PAGE_MAPPING, "Initialization finished");
    printf("Ration of free blocks available After Warmup %f nFreeBlocks %u Pages %lu\n",freeBlockRatio(),nFreeBlocks,nFreeBlocks*param.pagesInBlock);;

nlogicalpagesinSSD=nTotalLogicalPages;
  return true;
}

void PageMapping::read(Request &req, uint64_t &tick) {
  uint64_t begin = tick;;
  
if((iclCount %500000 ==0 ) && printDeadOnArrival)
{
  DeadPagePercentage.push_back(returnDeadPagePErcent());
}

  if (req.ioFlag.count() > 0) {
    //addLbaToOptMap(req.lpn);
    if(!optimalreplacementPolicy)
    {
      addLbaToOptMap(req.lpn);
    }
    readInternal(req, tick);
    ftlStats.TotalFtlReadRequests++;
    
    debugprint(LOG_FTL_PAGE_MAPPING,
               "READ  | LPN %" PRIu64 " | %" PRIu64 " - %" PRIu64 " (%" PRIu64
               ")",
               req.lpn, begin, tick, tick - begin);

  }
  else {
    warn("FTL got empty request");
  }
  
                 


  tick += applyLatency(CPU::FTL__PAGE_MAPPING, CPU::READ);
 //printf(" AFter REad %lu Cached Page Count %lu\n",req.lpn,getBlockCachedPageCount());
 

}
void PageMapping :: undoCached(uint64_t lpn)
{
  auto mappingList =
      table.find(lpn);  // mappinglist is the iterator of the table.
  std::unordered_map<uint32_t, Block>::iterator block;


  if (mappingList != table.end()) {// agr entry mil gayi
    //printf("In write Mapping found for the LPN %lu\n",req.lpn);
    for (uint32_t idx = 0; idx < bitsetSize; idx++) {
     
        auto &mapping = mappingList->second.at(idx);

        if (mapping.first < param.totalPhysicalBlocks &&  //blockid and 
            mapping.second < param.pagesInBlock) { // page id
            block = blocks.find(mapping.first);
            if(block !=blocks.end())
            {
               undoCaching++;
               block->second.deCache(mapping.second);
               
            }
      }
    }
  }
  
}
float PageMapping::returnDeadPagePErcent()
{
  
                size_t totalEntries = cmt.size();
                size_t countAccessCountOne = 0;

                for (const auto& entry : cmt) {
                    if (entry.second.cmtEntryAccessCount == 1) {
                        countAccessCountOne++;
                    }
                }

                // Calculate the percentage
                return (countAccessCountOne * 100.0) / totalEntries;
            

}
void PageMapping::addLbaToOptMap(uint64_t lpn)
{
  /*auto it = optMap.find(lpn);

    // If the key is present, append the entry to the vector
    if (it != optMap.end()) {
        it->second.push_back(iclCount);
    } else {
        // If the key is not present, create a new vector with the entry and insert it into the mapq
        optMap[lpn] = {iclCount};
    }*/
    auto it = AddressReuseDistanceMap.find(lpn);
     uint64_t reuseDistance=0;
    // If the key is present, append the entry to the vector
    if (it != AddressReuseDistanceMap.end()) {
       // it->second.push_back(iclCount);
            reuseDistance=iclCount- it->second;
            if(reuseDistance >= 8196 && reuseDistance < 16391)
            {
          AddressReuseDistanceClusters[16000]++;
            }
          else if(reuseDistance >= 16392 && reuseDistance < 32783)
          {
          AddressReuseDistanceClusters[32000]++;
            }
            else if(reuseDistance >= 32784 && reuseDistance < 65567)
          {
          AddressReuseDistanceClusters[64000]++;
            }
        else if(reuseDistance >= 65568 )
          {
          AddressReuseDistanceClusters[131136]++;
            }
            else{
              AddressReuseDistanceClusters[4000]++;
            }
          it->second = iclCount;
    } else {
        // If the key is not present, create a new vector with the entry and insert it into the mapq
        AddressReuseDistanceMap[lpn] = {iclCount};
    }
}
void PageMapping::write(Request &req, uint64_t &tick) {
  uint64_t begin = tick;
if((iclCount % 500000 ==0 ) && printDeadOnArrival)
{
  DeadPagePercentage.push_back(returnDeadPagePErcent());
}

  if (req.ioFlag.count() > 0) {
    if(!optimalreplacementPolicy)
    {
      addLbaToOptMap(req.lpn);
    }
    writeInternal(req, tick);
        //addLbaToOptMap(req.lpn);

        ftlStats.TotalFtlWriteRequests++;
    debugprint(LOG_FTL_PAGE_MAPPING,
               "WRITE | LPN %" PRIu64 " | %" PRIu64 " - %" PRIu64 " (%" PRIu64
               ")",
               req.lpn, begin, tick, tick - begin);

  }
  else {
    warn("FTL got empty request");
  }
      

  tick += applyLatency(CPU::FTL__PAGE_MAPPING, CPU::WRITE);
}

void PageMapping::trim(Request &req, uint64_t &tick) {
  uint64_t begin = tick;

  trimInternal(req, tick);

  debugprint(LOG_FTL_PAGE_MAPPING,
             "TRIM  | LPN %" PRIu64 " | %" PRIu64 " - %" PRIu64 " (%" PRIu64
             ")",
             req.lpn, begin, tick, tick - begin);

  tick += applyLatency(CPU::FTL__PAGE_MAPPING, CPU::TRIM);
}

void PageMapping::format(LPNRange &range, uint64_t &tick) {  //
  PAL::Request req(param.ioUnitInPage);
  std::vector<uint32_t> list;

  req.ioFlag.set();

  for (auto iter = table.begin(); iter != table.end();) {
    if (iter->first >= range.slpn && iter->first < range.slpn + range.nlp) {
      auto &mappingList = iter->second;

      // Do trim
      for (uint32_t idx = 0; idx < bitsetSize; idx++) {
        auto &mapping = mappingList.at(idx);
        auto block = blocks.find(mapping.first);

        if (block == blocks.end()) {
          panic("Block is not in use");
        }

        block->second.invalidate(mapping.second, idx);

        // Collect block indices
        list.push_back(mapping.first);
      }

      iter = table.erase(iter);
    }
    else {
      iter++;
    }
  }

  // Get blocks to erase
  std::sort(list.begin(), list.end());  // victim block
  auto last = std::unique(list.begin(), list.end());
  list.erase(last, list.end());

  // Do GC only in specified blocks
  doGarbageCollection(list, tick);
  

  tick += applyLatency(CPU::FTL__PAGE_MAPPING, CPU::FORMAT);
}

Status *PageMapping::getStatus(uint64_t lpnBegin, uint64_t lpnEnd) {
  status.freePhysicalBlocks = nFreeBlocks;

  if (lpnBegin == 0 && lpnEnd >= status.totalLogicalPages) {
    status.mappedLogicalPages = table.size();
  }
  else {
    status.mappedLogicalPages = 0;

    for (uint64_t lpn = lpnBegin; lpn < lpnEnd; lpn++) {
      if (table.count(lpn) > 0) {
        status.mappedLogicalPages++;
      }
    }
  }

  return &status;
}

float PageMapping::freeBlockRatio() {
  
  return (float)nFreeBlocks / param.totalPhysicalBlocks;
  
}

void PageMapping::resetBlockAccesCount()
{
      // Check if 5 minutes have passed
            for (auto &block : blocks) {
              block.second.setBlockAccessCountZero();
            }


            // Update the start time to the current time
        

      
}
uint32_t PageMapping::convertBlockIdx(uint32_t blockIdx) {
  return blockIdx % param.pageCountToMaxPerf;
}


uint32_t PageMapping::getFreeBlock(uint32_t idx) {
  uint32_t blockIndex = 0;

  if (idx >= param.pageCountToMaxPerf) {
    panic("Index out of range");
  }
//printf("n Free blocks %u\n",nFreeBlocks);
  if (nFreeBlocks > 0) {
    // Search block which is blockIdx % param.pageCountToMaxPerf == idx
    auto iter = freeBlocks.begin();

    for (; iter != freeBlocks.end(); iter++) {
      blockIndex = iter->getBlockIndex();

      if (blockIndex % param.pageCountToMaxPerf == idx) {
        break;
      }
    }
        //printf("ReqBlockId %u gerblockid %u BlockSize %ld LastFreeBlocks %ld NfreeBlocks %ld\n",idx,blockIndex,blocks.size(),lastFreeBlock.size(),freeBlocks.size());
    // Sanity check
    if (iter == freeBlocks.end()) {
      // Just use first one
      iter = freeBlocks.begin();
      blockIndex = iter->getBlockIndex();
    }

    // Insert found block to block list
    if (blocks.find(blockIndex) != blocks.end()) {
      panic("Corrupted");
    }

    blocks.emplace(blockIndex, std::move(*iter));

    freeBlocks.erase(iter);
    nFreeBlocks--;
  }
  else {
  printf("GC count is %d IO Count : %ld\n",gcCounter,myIoCount);
    panic("No free block left");
  }

  return blockIndex;
}

uint32_t PageMapping::getLastFreeBlock(
    Bitset &iomap) {  // this is the information about the new fee block..
  if (!bRandomTweak || (lastFreeBlockIOMap & iomap).any()) {
    // Update lastFreeBlockIndex
    lastFreeBlockIndex++;

    if (lastFreeBlockIndex == param.pageCountToMaxPerf) {
      lastFreeBlockIndex = 0;
    }

    lastFreeBlockIOMap = iomap;
  }
  else {
    lastFreeBlockIOMap |= iomap;
  }

      auto it = std::find(erased_block_id.begin(), erased_block_id.end(), lastFreeBlock.at(lastFreeBlockIndex));
      if (it != erased_block_id.end()) {// if this block is already erased!
        uint32_t erased_block=lastFreeBlock.at(lastFreeBlockIndex);
        lastFreeBlock.at(lastFreeBlockIndex) = getFreeBlock(lastFreeBlockIndex);// get a new block//
        erased_block_id.erase(std::remove(erased_block_id.begin(), erased_block_id.end(), erased_block), erased_block_id.end());
          } 

  auto freeBlock = blocks.find(lastFreeBlock.at(lastFreeBlockIndex));

  // Sanity check
  if (freeBlock == blocks.end()) {
    panic("Corrupted");
  }

 
  if (freeBlock->second.getNextWritePageIndex() == param.pagesInBlock) {
    lastFreeBlock.at(lastFreeBlockIndex) = getFreeBlock(lastFreeBlockIndex);

    bReclaimMore = true;
  }

  return lastFreeBlock.at(lastFreeBlockIndex);// returns the block id of the last free block.
}

uint32_t PageMapping::getUpdatedLastFreeBlock() {
   
    uint32_t   blockIndexofLeastUsedBlockInFreeBlocks = findBlockWithLeastValidDirtyPagesFromVector();// this will return the id of the leastUsedBlock;

    // Sanity check
     // printf("Value Returned from fun %u\n",blockIndexofLeastUsedBlockInFreeBlocks);

    auto it = std::find(erased_block_id.begin(), erased_block_id.end(), lastFreeBlock.at(blockIndexofLeastUsedBlockInFreeBlocks));
      if (it != erased_block_id.end()) {// if this block is already erased!
        uint32_t erased_block=lastFreeBlock.at(blockIndexofLeastUsedBlockInFreeBlocks);
        lastFreeBlock.at(blockIndexofLeastUsedBlockInFreeBlocks) = getFreeBlock(blockIndexofLeastUsedBlockInFreeBlocks);// get a new block//
        erased_block_id.erase(std::remove(erased_block_id.begin(), erased_block_id.end(), erased_block), erased_block_id.end());
          } 
    auto freeBlock = blocks.find(lastFreeBlock.at(blockIndexofLeastUsedBlockInFreeBlocks));


    if (freeBlock == blocks.end()) {
        panic("Corrupted");
    }

    // If current free block is full, get next block
    if (freeBlock->second.getNextWritePageIndex() == param.pagesInBlock) {
        lastFreeBlock.at(blockIndexofLeastUsedBlockInFreeBlocks) = getFreeBlock(blockIndexofLeastUsedBlockInFreeBlocks);

        bReclaimMore = true;
    }
    
    // printf("Returned Block %u\n",lastFreeBlock.at(blockIndexofLeastUsedBlockInFreeBlocks));
    return lastFreeBlock.at(blockIndexofLeastUsedBlockInFreeBlocks);
}

uint32_t PageMapping::findBlockWithLeastValidDirtyPagesFromVector() {
   
    /*uint32_t returnId = 0;
   // uint32_t leastValidDirtyCount = std::numeric_limits<uint32_t>::max(); // Initialize with a high value
      uint32_t leastValidDirtyCount = 0;
    for(size_t i = 0; i < lastFreeBlock.size(); ++i)
    {
      auto block = blocks.find(lastFreeBlock.at(i));
       if (block != blocks.end()) {
            //const Block &blockObj = block->second;

            // Calculate the total of valid pages and dirty pages
            uint32_t validDirtyCount = block->second.getValidPageCount() +  block->second.getDirtyPageCount() ;

            // Update leastValidDirtyBlock if the current block has fewer valid + dirty pages
            if (validDirtyCount > leastValidDirtyCount) {
                leastValidDirtyCount = validDirtyCount;
                returnId=i;
            }
        }
    }

    return returnId;// this will be the blockId OF lEAST USED BLOCK..*/
   static uint32_t currentIndex = 0; // Keep track of the current index in lastFreeBlock
    static uint32_t currentPageInBlock = 0;

    // If all pages in the current block are written, move to the next block ID in lastFreeBlock
    if (currentPageInBlock >= param.pagesInBlock) {
        currentIndex = ((currentIndex+1)  % param.pageCountToMaxPerf)  ; // Move to the next index circularly
        currentPageInBlock = 0; // Reset page count for the new block
    }
    
    uint32_t returnId = currentIndex;
    currentPageInBlock++; // Increment page count for the current block
   // printf("Id Returned %u\n",returnId);
    return returnId; // Return the block ID from lastFreeBlock
}



void PageMapping::calculateVictimWeight(
    std::vector<std::pair<uint32_t, float>> &weight,const EVICT_POLICY policy,
    uint64_t tick) {
  float temp;
//printf("Check if you have entered in the old gc scheme\n");
  weight.reserve(blocks.size());
  //cachedWeight.reserve(blocks.size());

  switch (policy) {
    case POLICY_GREEDY:
    case POLICY_RANDOM:
    case POLICY_DCHOICE:
      for (auto &iter : blocks) {
        if (iter.second.getNextWritePageIndex() != param.pagesInBlock) {
          continue;
        }

        weight.push_back( {iter.first, iter.second.getValidPageCountRaw()});  // here they have direct
      
                                                    
      }
      break;
      case HOTBLOCK:
      for (auto &iter : blocks) {
        if (iter.second.getNextWritePageIndex() != param.pagesInBlock) {
          continue;
        }

        weight.push_back( {iter.first, iter.second.getBlockAccessCount()});  // here they have direct
      
                                                    
      }
      break;
    case POLICY_COST_BENEFIT:
      for (auto &iter : blocks) {
        if (iter.second.getNextWritePageIndex() != param.pagesInBlock) {
          continue;
        }

        temp = (float)(iter.second.getValidPageCountRaw()) / param.pagesInBlock;

        weight.push_back(
            {iter.first,
             temp / ((1 - temp) * (tick - iter.second.getLastAccessedTime()))});
      }

      break;
    default:
      panic("Invalid evict policy");;
  }
  
}
void PageMapping::calculateVictimCachedWeight(std::vector<std::pair<uint32_t, std::pair<uint32_t, uint32_t>>>&cachedWeight)
  {
      //printf
     
      cachedWeight.reserve(blocks.size());
      
      for (auto &iter : blocks) {
        if (iter.second.getNextWritePageIndex() != param.pagesInBlock) {
          continue;
        }

        //weight.push_back( {iter.first, iter.second.getValidPageCountRaw()});  // here they have direct
       cachedWeight.push_back(std::make_pair(iter.first, std::make_pair(iter.second.getCachedPageCount(), (iter.second.getValidPageCount()))));
        //cachedWeight.push_back(std::make_pair(iter.first, std::make_pair((iter.second.getValidPageCount()- iter.second.getCachedPageCount()), (iter.second.getValidPageCount())))); //pagesnotcached and valid pages
       // printf("Block Id %u AccCount %u\n",iter.second.getBlockIndex(),iter.second.getBlockAccessCount());
       ftlStats.cachedpagesfoundInAllBlocks +=iter.second.getCachedPageCount();
        //HotnessMeter[iter.second.getBlockAccessCount()]++;
                                                    
      }
      //printf("Size of cachedWeight %ld\n",cachedWeight.size());
  }


uint64_t PageMapping::getAccessCountOfLBA(uint64_t lpn)
{
  auto iter= globalLBaReuseMap.find(lpn);
  uint64_t temp=0;
  if(iter !=globalLBaReuseMap.end())
  {
    temp = iter->second;
  }
  return temp;
}
uint32_t PageMapping::getLPNEvictionFrequency(uint64_t lpn)
{
   auto iter= lbaEvictionFrequency.find(lpn);
  uint32_t temp=0;
  if(iter!=lbaEvictionFrequency.end())
  {
    temp = iter->second;
  }
  return temp;
}
 void PageMapping::findHotAccessBlock(std::vector<std::pair<uint32_t, std::pair<uint32_t, uint32_t>>> &blockAccessCountVector)
 {
      blockAccessCountVector.reserve(blocks.size());
      
      
        for (auto &iter : blocks) {
        if (iter.second.getNextWritePageIndex() != param.pagesInBlock) {
          continue;
        }

        blockAccessCountVector.push_back(std::make_pair(iter.first, std::make_pair(iter.second.getBlockAccessCount(), (iter.second.getCachedPageCount())))); // here they have direct
      
                                                    
      }
                                                    
      
 }

 void PageMapping:: callBlockResetFunction(uint64_t & tick)
 {
          uint64_t elapsedTime= tick-startingTick;
        //printf("ElapsedTime: %lu \n",elapsedTime);

      if (elapsedTime >= 10000000000000) { 

           // resetBlockAccesCount();
            startingTick = tick;
      }
 }
void PageMapping::selectVictimBlock(                   
    std::vector<uint32_t> &list,
    uint64_t &tick) {  // this is the list of of items it will take with itself
  static const GC_MODE mode = (GC_MODE)conf.readInt(
      CONFIG_FTL, FTL_GC_MODE);  // some configuration being read
  static const EVICT_POLICY policy =
      (EVICT_POLICY)conf.readInt(CONFIG_FTL, FTL_GC_EVICT_POLICY);
  static uint32_t dChoiceParam =
      conf.readUint(CONFIG_FTL, FTL_GC_D_CHOICE_PARAM);
  uint64_t nBlocks = conf.readUint(
      CONFIG_FTL, FTL_GC_RECLAIM_BLOCK);           // claiming n blocks to free
  std::vector<std::pair<uint32_t, float>> weight;  
  std::vector<std::pair<uint32_t, std::pair<uint32_t, uint32_t>>> blockAccessCountVector;// weight of each block
  std::vector<std::pair<uint32_t, std::pair<uint32_t, uint32_t>>> cachedWeight;//ok

  list.clear();

  // Calculate number of blocks to reclaim
  if (mode == GC_MODE_0) {
    // DO NOTHING
  }
  else if (mode == GC_MODE_1) {
    static const float t = conf.readFloat(CONFIG_FTL, FTL_GC_RECLAIM_THRESHOLD);

    nBlocks = param.totalPhysicalBlocks * t - nFreeBlocks;
    //printf("OUt side recliam blocks are ");;
  }
  else {
    panic("Invalid GC mode");
  }

  // reclaim one more if last free block fully used
  if (bReclaimMore) {
    //nBlocks += param.pageCountToMaxPerf;// change is done here

    bReclaimMore = false;
  }

  // Calculate weights of all blocks
 //   // sending the weight vector, policy and the time
if(policy == CACHED_GC )
 {
                calculateVictimCachedWeight(cachedWeight);// this should call the cachedGC Algorithm
              }
else
{
              /*if(policy==HOTBLOCK)
              {
              findHotAccessBlock(blockAccessCountVector);// this will call the hot block victim Selection algorithm
                }
              else
              {}*/
                if(pageAwareofCache)
                {
                  calculateVictimCachedWeight(cachedWeight);

                }
                else
                {
                      calculateVictimWeight(weight,policy, tick);// this will call the basic greedy victim selection
                }
                

              
 }

  if (policy == POLICY_RANDOM || policy == POLICY_DCHOICE) {
    uint64_t randomRange =
        policy == POLICY_RANDOM ? nBlocks : dChoiceParam * nBlocks;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint64_t> dist(0, weight.size() - 1);
    std::vector<std::pair<uint32_t, float>> selected;
    while (selected.size() < randomRange) {
      uint64_t idx = dist(gen);

      if (weight.at(idx).first < std::numeric_limits<uint32_t>::max()) {
        selected.push_back(weight.at(idx));
        weight.at(idx).first = std::numeric_limits<uint32_t>::max();
      }
    }

    weight = std::move(selected);
  }
    if(policy==CACHED_GC && !(cachedWeight.empty()))
      {

      nBlocks = MIN(nBlocks, cachedWeight.size());
        for (uint64_t i = 0; i < nBlocks; i++) {
        //list.push_back(cachedWeight.at(i).first);
         //uint32_t victimBlockIndex = findVictimBlock(cachedWeight);
        list.push_back(findVictimBlock(cachedWeight));
       }




      }
      else
      { // for other caching schemes

     
     if(pageAwareofCache)
     {
      nBlocks = MIN(nBlocks, cachedWeight.size());
        for (uint64_t i = 0; i < nBlocks; i++) {
        //list.push_back(cachedWeight.at(i).first);
         //uint32_t victimBlockIndex = findVictimBlock(cachedWeight);
        list.push_back(findVictimBlock(cachedWeight));
       }

     }
     else{
     std::sort(
      weight.begin(), weight.end(),
      [](std::pair<uint32_t, float> a, std::pair<uint32_t, float> b) -> bool {
        return a.second < b.second;
      });
  
      nBlocks = MIN(nBlocks, weight.size());
        for (uint64_t i = 0; i < nBlocks; i++) {
        list.push_back(weight.at(i).first);
        }
      }
      
      }// elsepart of main
  tick += applyLatency(CPU::FTL__PAGE_MAPPING, CPU::SELECT_VICTIM_BLOCK);
}
uint32_t PageMapping::findVictimBlock(std::vector<std::pair<uint32_t, std::pair<uint32_t, uint32_t>>>& cachedWeight) {
    /*uint32_t LN_NTBR = cachedWeight[0].second.second - cachedWeight[0].second.first; // valid pages minus those pages not cached in i,e need to read pages
    uint32_t Temp_V = cachedWeight[0].second.second;  // valid pages
    uint32_t Temp_B_N =  cachedWeight[0].first;;


    for (size_t i = 1; i < cachedWeight.size(); ++i) {
      auto it = std::find(erased_block_id.begin(), erased_block_id.end(), cachedWeight[i].first);
              if (it == erased_block_id.end()) {
                continue;
          }
        uint32_t N_NTBR = cachedWeight[i].second.second - cachedWeight[i].second.first;  // new need to read pages
            //printf("needtoreed %u and %u\n",N_NTBR,Temp_B_N);
        if (LN_NTBR < N_NTBR) {
             // Do nothing
        } else if (LN_NTBR > N_NTBR) {
            LN_NTBR = N_NTBR;
            Temp_V = cachedWeight[i].second.second;
            Temp_B_N = cachedWeight[i].first;
        } else {
            if (Temp_V < cachedWeight[i].second.second) {
                Temp_V = cachedWeight[i].second.second;
                Temp_B_N = cachedWeight[i].first;;
            }
        }
    }

    return Temp_B_N; // Returning index of the victim block
    auto comparePagesToRead = [](const std::pair<uint32_t, std::pair<uint32_t, uint32_t>>& a,
                                 const std::pair<uint32_t, std::pair<uint32_t, uint32_t>>& b) {
        uint32_t N_NTBR_a = a.second.second - a.second.first;
        uint32_t N_NTBR_b = b.second.second - b.second.first;

        if (N_NTBR_a == N_NTBR_b) {
            uint32_t Temp_V_a = a.second.second;
            uint32_t Temp_V_b = b.second.second;
            return Temp_V_a < Temp_V_b;
        } else {
            return N_NTBR_a < N_NTBR_b;
        }
    };

    // Use the locally defined comparison function to sort cachedWeight
    std::sort(cachedWeight.begin(), cachedWeight.end(), comparePagesToRead);*/
    std::sort(cachedWeight.begin(), cachedWeight.end(),
    [](const std::pair<uint32_t, std::pair<uint32_t, uint32_t>>& a,
       const std::pair<uint32_t, std::pair<uint32_t, uint32_t>>& b) {
        uint32_t validPagesA = a.second.second;
        uint32_t cachedPagesA = a.second.first;
        uint32_t validPagesB = b.second.second;
        uint32_t cachedPagesB = b.second.first;

        // Sort by minimum valid pages (ascending)
        if (validPagesA != validPagesB) {
          
            return validPagesA < validPagesB;
        }

        // In the case of a tie, sort by maximum cached pages (descending)
        return cachedPagesA > cachedPagesB;
    });
    return cachedWeight[0].first;
}
SSDInfo PageMapping:: getSSDInternalInfo(uint32_t blockId)
{
  SSDInfo internal;
  
  internal.channels=blockId % ssdInternals[0];
  blockId = blockId/ssdInternals[0];
  internal.Chips= blockId % ssdInternals[1];
  blockId = blockId/ssdInternals[1];
  internal.Dies=blockId % ssdInternals[2];
  blockId = blockId/ssdInternals[2];
  internal.Planes= blockId % ssdInternals[3];
  blockId = blockId/ssdInternals[3];
  return internal;
}
void PageMapping::writeToLeastusedBlock(uint64_t lpn, uint64_t &tick)
{

      uint64_t beginAt=tick;
      PAL::Request req(param.ioUnitInPage);
    Bitset bit(param.ioUnitInPage);
   auto freeBlock = blocks.find(getUpdatedLastFreeBlock());
    uint32_t newBlockIdx = freeBlock->first;
    for (uint32_t idx = 0; idx < bitsetSize; idx++) {
          if (bit.test(idx)) {
            // Invalidate
            

            auto mappingList = table.find(lpn);

            if (mappingList == table.end()) {
              panic("Invalid mapping table entry");
            }

            pDRAM->read(&(*mappingList), 8 * param.ioUnitInPage, tick);

            auto &mapping = mappingList->second.at(idx);

            uint32_t newPageIdx = freeBlock->second.getNextWritePageIndex(idx);

            mapping.first = newBlockIdx;
            mapping.second = newPageIdx;

            freeBlock->second.write(newPageIdx, lpn, idx, beginAt);

            // Issue Write
            req.blockIndex = newBlockIdx;
            req.pageIndex = newPageIdx;

            if (bRandomTweak) {
              req.ioFlag.reset();
              req.ioFlag.set(idx);
            }
            else {
              req.ioFlag.set();
            }

            //writeRequests.push_back(req);// this is the write request to the pal;
        pPAL->write(req, beginAt);
            stat.validPageCopies++;
          }
        }


}

void PageMapping::doGarbageCollection(std::vector<uint32_t> &blocksToReclaim,
                                      uint64_t &tick) {
  PAL::Request req(param.ioUnitInPage);
  std::vector<PAL::Request> readRequests;
  std::vector<PAL::Request> writeRequests;
  std::vector<PAL::Request> eraseRequests;
 std::vector<ICL::Request> writeRequests1;
  std::vector<uint64_t> lpns;
   std::vector<uint64_t> lpa_list;
   std::vector<uint64_t>lpnListForCache;
   std::vector<uint64_t> CachedValidLPN;
  Bitset bit(param.ioUnitInPage);
  uint64_t beginAt=tick;// yeha maine khud se kiya
  uint64_t readFinishedAt = tick;
  uint64_t writeFinishedAt = tick;
  uint64_t eraseFinishedAt = tick;
  victimBlockInfo vbi;
  if(iclCount > SimlationIORequests) return;
 // std::exit(0);
static const EVICT_POLICY policy =(EVICT_POLICY)conf.readInt(CONFIG_FTL, FTL_GC_EVICT_POLICY);

  if (blocksToReclaim.size() == 0) { // if no block to be reclaimed.
    return;
  }
gcCounter++;
   

if(policy== CACHED_GC) //@CachedGC 
{
   // printf("Tick %lu ICLCount %lu GcCount %d\n",tick,iclCount,gcCounter);
  for (auto &iter : blocksToReclaim) {
              // printf("Indexof ReturnedBlock %u\n",iter);

    auto block = blocks.find(iter);
        //printf("Block: %u GC: %d IO %ld EraseList %ld PagesToMove %lu ICLCount: %ld FreeBlockRatio %f\n",block->second.getBlockIndex(),gcCounter,myIoCount,eraser,(param.pagesInBlock-(block->second.getValidPageCount()-block->second.getCachedPageCount())),iclCount,freeBlockRatio());
  //  printf("VBlock: %u GC: %d VPages: %u AcC %u CPages %u IPages %u ICLCount %lu Tick %lu PagesToMove %u\n",block->second.getBlockIndex(),gcCounter,block->second.getValidPageCount(),block->second.getBlockAccessCount(),block->second.getCachedPageCount(),block->second.getDirtyPageCount(),iclCount,tick,(block->second.getValidPageCount()-block->second.getCachedPageCount()));
    if (block == blocks.end()) {
      panic("Invalid block");
    }
//eraser++;
    // Copy valid pages to free block
    stat.cachedPageCount +=block->second.getCachedPageCount();
    stat.totalValidPagesMovement +=block->second.getValidPageCount();

    for (uint32_t pageIndex = 0; pageIndex < param.pagesInBlock; pageIndex++) {// for each page in a block
      // Valid?
       if(!block->second.isCached(pageIndex) && block->second.isPageValid(pageIndex))/// @@point to note here that if the block is not cached
      {
      if (block->second.getPageInfo(pageIndex, lpns, bit)) {
        
      // believe that you got cached and valid page 
     // printf("LPN of:%u is %lu\n",pageIndex,lpns[0]);
     
        if (!bRandomTweak) {
          bit.set();
        }
        
        stat.pagesNotCachedButValid++;
        // Issue Read
        req.blockIndex = block->first;
        req.pageIndex = pageIndex;
        req.ioFlag = bit;
        lpa_list.push_back(lpns[0]);

        readRequests.push_back(req);// add it to the read request queue what ever page is there ..here check the structure//
        //**************************This is the portion of GreedyGC********************

    if(blockToBlockMovement){
         //if((getAccessCountOfLBA(lpns[0]) <= 2 ) || (getLPNEvictionFrequency(lpns[0]) <= 2))
         if((getLPNEvictionFrequency(lpns[0]) <= 2))
          {

               // Update mapping table
        auto freeBlock = blocks.find(getLastFreeBlock(bit));// it will return the block which is ready to accept the IO request.
        //auto freeBlock = blocks.find(getUpdatedLastFreeBlock());
        uint32_t newBlockIdx = freeBlock->first; // new free block id

        for (uint32_t idx = 0; idx < bitsetSize; idx++) {
          if (bit.test(idx)) {
            // Invalidate
            block->second.invalidate(pageIndex, idx);

            auto mappingList = table.find(lpns.at(idx));

            if (mappingList == table.end()) {
              panic("Invalid mapping table entry");
            }

            pDRAM->read(&(*mappingList), 8 * param.ioUnitInPage, tick);

            auto &mapping = mappingList->second.at(idx);

            uint32_t newPageIdx = freeBlock->second.getNextWritePageIndex(idx);

            mapping.first = newBlockIdx;
            mapping.second = newPageIdx;

            freeBlock->second.write(newPageIdx, lpns.at(idx), idx, beginAt);
              stat.victimToFreeMovements++;
            // Issue Write
            req.blockIndex = newBlockIdx;
            req.pageIndex = newPageIdx;

            if (bRandomTweak) {
              req.ioFlag.reset();
              req.ioFlag.set(idx);
            }
            else {
              req.ioFlag.set();
            }

            writeRequests.push_back(req);// this is the write request to the pal;

            stat.validPageCopies++;
          }
        }

        



          }
          else
          {
            lpnListForCache.push_back(lpns[0]);
          }

        //********************************EndOfGreedyGCPortion

      }// end of blockToBlockMovements


        

        stat.validSuperPageCopies++;
      }
      
      }
      if(block->second.isCached(pageIndex))
      {
         // mynumber++;
          CachedValidLPN.push_back(lpns[0]);// there will be used for updating the dirty bit of each lpn.
          stat.pagesUpdatedDirty++;
      }
    }// till here it will be the code for the eviction lines
//printf("size of lpsn is %ld and myNumber is %d\n",lpa_list.size(),mynumber);
    // Erase block
    req.blockIndex = block->first;
    req.pageIndex = 0;
    req.ioFlag.set();

    eraseRequests.push_back(req);
  }

  }// end of cachedGC policy
  /*This the garbage collection for other victimSelection Policies*/
else // Default Greedy GC
    {
    
       for (auto &iter : blocksToReclaim) {
    auto block = blocks.find(iter);
   // printf("VBlock: %u GC: %d VPages: %u AcC %u CPages %u IPages %u ICLCount %lu Tick %lu PagesToMove %u\n",block->second.getBlockIndex(),gcCounter,block->second.getValidPageCount(),block->second.getBlockAccessCount(),block->second.getCachedPageCount(),block->second.getDirtyPageCount(),iclCount,tick,(block->second.getValidPageCount()- block->second.getCachedPageCount()));

    if (block == blocks.end()) {
      panic("Invalid block");
    }
           
    // Copy valid pages to free block
    //int pageIndexer=0;
              stat.cachedPageCount +=block->second.getCachedPageCount();
              stat.totalValidPagesMovement +=block->second.getValidPageCount();
    
        for (uint32_t pageIndex = 0; pageIndex < param.pagesInBlock; pageIndex++) {// for each page in the victim block
      // Valid?
      if (block->second.getPageInfo(pageIndex, lpns, bit)) {
      if(block->second.isPageValid(pageIndex))
      {
          
      
        if (!bRandomTweak) {
          bit.set();
        }

            // Retrive free block
            
        auto freeBlock = blocks.find(getLastFreeBlock(bit));// it will return the block which is ready to accept the IO request.
        //auto freeBlock = blocks.find(getUpdatedLastFreeBlock());

       // printf("block Called %u PageIndex %d\n",freeBlock->second.getBlockIndex(),pageIndexer++);

        // Issue Read
        req.blockIndex = block->first;
        req.pageIndex = pageIndex;
        req.ioFlag = bit;

        readRequests.push_back(req); /// this much of the data i have to read

        // Update mapping table
        uint32_t newBlockIdx = freeBlock->first; // new free block id

        for (uint32_t idx = 0; idx < bitsetSize; idx++) {
          if (bit.test(idx)) {
            // Invalidate
            block->second.invalidate(pageIndex, idx);

            auto mappingList = table.find(lpns.at(idx));

            if (mappingList == table.end()) {
              panic("Invalid mapping table entry");
            }

            pDRAM->read(&(*mappingList), 8 * param.ioUnitInPage, tick);

            auto &mapping = mappingList->second.at(idx);

            uint32_t newPageIdx = freeBlock->second.getNextWritePageIndex(idx);

            mapping.first = newBlockIdx;
            mapping.second = newPageIdx;
            auto cmtkey= cmt.find(lpns.at(idx));
            if(cmtkey != cmt.end())
            {
              cmtkey->second.block_id=mapping.first ;
              cmtkey->second.page_Id=mapping.second ;
              
            }

            freeBlock->second.write(newPageIdx, lpns.at(idx), idx, beginAt);

            // Issue Write
            req.blockIndex = newBlockIdx;
            req.pageIndex = newPageIdx;

            if (bRandomTweak) {
              req.ioFlag.reset();
              req.ioFlag.set(idx);
            }
            else {
              req.ioFlag.set();
            }

            writeRequests.push_back(req);// this is the write request to the pal;

            stat.validPageCopies++;
          }
        }

        stat.validSuperPageCopies++;
      }
      /*else  // for all non valid pages in the 
      {
            auto lpnInvalid= table.find(lpns.at(0));
            if(lpnInvalid !=table.end())
            {
              stat.entreisPresentInmappingtableNotupdated++;
              auto x = lpnInvalid->second.at(0);
              if( x.first == iter)
              {
               stat.entriesbelongingTothisblock++;
                //table.erase(lpnInvalid);
              }
            }
      }*/
    }// this is check the condition for valid page only
    } //iterator for
       
    // Erase block
    req.blockIndex = block->first;
    req.pageIndex = 0;
    req.ioFlag.set();

    eraseRequests.push_back(req);
  }
    

  }

  // Do actual I/O here
  // This handles PAL2 limitation (SIGSEGV, infinite loop, or so-on)
  if(policy== CACHED_GC)
  {
    //int k=0;
    SimpleSSD::ICL::Request CacheWriteReq;
    size_t size = readRequests.size();
    pageMoveBucket[size]++;  //totalPagesMovedFromBlockToCache +=size;//bucketWise entering the number of pages from blockto Cache

for (auto &iter : CachedValidLPN) { //updating dirty Entries in Cache;
      beginAt = tick;
      globalCache->updateDirty(iter,beginAt);// this function updates the entries in cache as dirty.
      RemoveEntryFromTable(iter,beginAt);
      writeFinishedAt = MAX(writeFinishedAt, beginAt);
    //tick=writeFinishedAt;
  }
  for (size_t i = 0; i < size; i++) {//Reading the valid Pages
    auto& iter = readRequests[i];
    beginAt = tick;
    pPAL->read(iter, beginAt);
    readFinishedAt = MAX(readFinishedAt, beginAt); 
    }
    if(blockToBlockMovement)
    {
              for (auto &iter : writeRequests) {
              beginAt = readFinishedAt;
          //totalPagesMovedFromBlockToBlock++;
              pPAL->write(iter, beginAt);

              writeFinishedAt = MAX(writeFinishedAt, beginAt);
            }
    }

  for (auto &iter : eraseRequests) {
    beginAt = readFinishedAt;
    //erase_counter++;
    eraser++;
    erased_block_id.push_back(iter.blockIndex);
    
    eraseInternal(iter, beginAt);
   
    eraseFinishedAt = MAX(eraseFinishedAt, beginAt);
  }
  if(blockToBlockMovement)
    {
    for (size_t i = 0; i < lpnListForCache.size(); i++) { // there are those requests that will  be written to the cache when there is 
    //auto& iter = readRequests[i];
    uint64_t lpn = lpnListForCache[i];
   totalPagesMovedFromBlockToCache++;
   // check the access frequency.
    movePagesToCache.push_back(lpn);
    //tick=eraseFinishedAt;
    //beginAt = tick;
    RemoveEntryFromTable(lpn,tick);
    }
    }
    else
    {

  for (size_t i = 0; i < size; i++) { // there are those requests that will  be written to the cache when there is 
    //auto& iter = readRequests[i];
    uint64_t lpn = lpa_list[i];
    
      totalPagesMovedFromBlockToCache++;
   // check the access frequency.
    movePagesToCache.push_back(lpn);
    //tick=eraseFinishedAt;
    //beginAt = tick;
    RemoveEntryFromTable(lpn,tick);
    }
    

    
   
}
if(movePagesToCache.size())
{
  putPagesInCache=true;
}
//printEvictLines();
  
  }
  else// else part of cachedGC
  {
    
    for (auto &iter : readRequests) {
    beginAt = tick;
  
    pPAL->read(iter, beginAt);

    readFinishedAt = MAX(readFinishedAt, beginAt);
      }
//std::exit(0);
   // size_t size = readRequests.size();
       // printf("Pages toWrite to new block %ld\n",size);
   // pageMoveBucket[size]++; 
    //totalPagesMovedFromBlockToBlock +=size;// here it is totalpages moved from victimBlock to FreeBlocks
  for (auto &iter : writeRequests) {
    beginAt = readFinishedAt;
 //totalPagesMovedFromBlockToBlock++;
    pPAL->write(iter, beginAt);

    writeFinishedAt = MAX(writeFinishedAt, beginAt);
  }

  for (auto &iter : eraseRequests) {
    beginAt = readFinishedAt;
    erased_block_id.push_back(iter.blockIndex);
   //eraseMapBucket[(iter.blockIndex % param.pageCountToMaxPerf)]++;

    eraseInternal(iter, beginAt);
    eraser++;
    //printf("Eraser Value %ld\n",eraser);
    eraseFinishedAt = MAX(eraseFinishedAt, beginAt);
      }
    
  }//else part of cacheGc
  tick = MAX(writeFinishedAt, eraseFinishedAt);
  tick += applyLatency(CPU::FTL__PAGE_MAPPING, CPU::DO_GARBAGE_COLLECTION);
}
void PageMapping::optimalRepalcementPolicy()
{
  
   if (cmt.size() >= cmtSize) { // Eviction needed
   std::vector<uint64_t> cmtRemovalEntries;
     // printf("Entries in CM are ..\n");
  /*for (const auto &entry : cmt) {
     auto optMapEntry = optMap.find(entry.first); 
      if (optMapEntry != optMap.end() )
      {

    printf("%lu - %lu \t",entry.first,optMapEntry->second[0]);
      }
  }
printf("\n");*/
   
        uint64_t lruKey = 0;
        uint64_t maxFirstElement = 0;
        uint64_t currentAccessCount=0;
       // uint64_t evictionKeyAccessCount=0;
        // Find the least recently used entry
        for (const auto &entry : cmt) {
            uint64_t currentKey = entry.first;
             currentAccessCount = entry.second.cmtEntryAccessCount;
                uint64_t icl =iclCount;
            // Check if the entry exists in the optMap
           // printf("cmtSizeee %ld\n",cmt.size());
            auto optMapEntry = optMap.find(currentKey); 
            if (optMapEntry != optMap.end() ) {
                // Compare the first element of the vector
            // printf("(LPn %lu - cmt %lu Icl %lu opicl %lu)\t",currentKey,entry.second.iclEntry,iclCount,optMapEntry->second[0]);

                auto& optMapEntryVector = optMapEntry->second;
                optMapEntryVector.erase(
                    std::remove_if(optMapEntryVector.begin(), optMapEntryVector.end(),
                                   [icl](uint64_t val) { return val <= icl; }),
                    optMapEntryVector.end());
                   // printf("(LPn %lu - cmt %lu Icl %lu opicl %lu)\t",currentKey,entry.second.iclEntry,iclCount,optMapEntry->second[0]);
                  //printf( "\nOptmalSecond %lu %lu %lu optimalvector %lu ICL %lu\n",optMapEntry->second[0],maxFirstElement,currentKey,optMapEntryVector[0],iclCount);

                if(optMapEntry->second.empty())
                {
                  //cmt.erase(optMapEntry->first);
                 // printf("Erased %lu cmtSize %ld\n",optMapEntry->first,cmt.size());
                  cmtRemovalEntries.push_back(optMapEntry->first);
                 // continue;

                      }
                      else{
                if(optMapEntry->second[0] > maxFirstElement) {
                    maxFirstElement = optMapEntry->second[0];
                    lruKey = currentKey;
                    //evictionKeyAccessCount=currentAccessCount;
                  //cmtRemovalEntries.push_back(lruKey);

                                   // printf("(LPn %lu - cmt %lu Icl %lu opicl %lu)\t",currentKey,entry.second.iclEntry,iclCount,optMapEntry->second[0]);

                }
                      }
                //optMapEntry
            }
          
        }
          cmtRemovalEntries.push_back(lruKey);
        for(auto entry : cmtRemovalEntries)
        {
          //cmt.erase(entry);
        
               // printf("\n");
      
            evictedCmtEntries[entry] = currentAccessCount; // victimCache
            // printf("ValueEvicted %lu Size of EvictedMap %ld AccessCountEvictedEntry %u\n",lruKey,evictedCmtEntries.size(),maxFirstElement);
            if (maxFirstElement == 1) {
                cmtStats.entriesWithOneAccessCount++;
            }
            cmtStats.cmtEvictions++;
            // cmtEntryEvictionAccessFrequency[maxFirstElement]++;

            // Remove the first element from the vector in optMap
            
            cmt.erase(entry); // Remove LRU entry from the CMT
           // printf("Eviction %lu Key %lu cmtSize %ld\n",cmtStats.cmtEvictions,entry,cmt.size());

}
        
    }
}

void PageMapping:: EvictFromShadowCache()
{
  if (shadowwCache.size() >= shadowwCacheSize) { // Eviction needed
            uint64_t oldestTime = std::numeric_limits<uint64_t>::max();
            uint64_t lruKey = 0;
           // uint32_t keyAccessCount=0;

            // Find the least recently used entry
            for (const auto &entry : shadowwCache) {
                if (entry.second.lastAccessedTime < oldestTime) {
                    oldestTime = entry.second.lastAccessedTime;
                    lruKey = entry.first;
                   // keyAccessCount=entry.second.cmtEntryAccessCount;
                }
            }

            if (lruKey != 0) {
                cmtStats.EvictionsFromShadowCache++;
                shadowwCache.erase(lruKey); // Remove LRU entry from the CMT
            }
        }
}
void PageMapping::evictEntryFromCMT()
{
   if (cmt.size() >= cmtSize) { // Eviction needed
            uint64_t oldestTime = std::numeric_limits<uint64_t>::max();
            uint64_t lruKey = 0;
            uint32_t keyAccessCount=0;
                //displayCMt();
            // Find the least recently used entry
            for (const auto &entry : cmt) {
                if (entry.second.lastAccessedTime < oldestTime) {
                    oldestTime = entry.second.lastAccessedTime;
                    lruKey = entry.first;
                    keyAccessCount=entry.second.cmtEntryAccessCount;
                }
            }

            if (lruKey != 0) {
              

              if(keyAccessCount == 1)
              {
              evictedCmtEntries[lruKey]++;// victimCache
              
               cmtStats.entriesWithOneAccessCount++;

              }
           
              cmtStats.cmtEvictions++;
              cmtEntryEvictionAccessFrequency[keyAccessCount]++;
             // printf("\nEvictedKey %lu AccessCount %u \n\n",lruKey,keyAccessCount);
                cmt.erase(lruKey); // Remove LRU entry from the CMT
            }
        }
}
void PageMapping::readInternal(Request &req, uint64_t &tick) {
  PAL::Request palRequest(req);
  uint64_t beginAt;
  uint64_t finishedAt = tick;
  //uint64_t evictedEntryPreviousValue=0;
   totalFtlReads++;
bool deadonArrival=false;

      cmtStats.cmtReadRequests++;
   auto cmtEntry = cmt.find(req.lpn); // lookup cmt
  if(cmtEntry != cmt.end() && enableCMT)
  {
    if (bRandomTweak) {
      pDRAM->read(&(*cmtEntry), 8 * req.ioFlag.count(), tick);
    }
    else {
      pDRAM->read(&(*cmtEntry), 8, tick);
    }
      cmtEntry->second.lastAccessedTime=tick;//updating cmt
      cmtEntry->second.cmtEntryAccessCount++;
      cmtStats.cmtReadRequestsHits++;
      cmtEntry->second.iclEntry=iclCount;
    for (uint32_t idx = 0; idx < bitsetSize; idx++) {
      if (req.ioFlag.test(idx) || !bRandomTweak) {
        auto &mapping = cmtEntry->second;

        if (mapping.block_id < param.totalPhysicalBlocks &&
            mapping.page_Id < param.pagesInBlock) {
          palRequest.blockIndex = mapping.block_id;
          palRequest.pageIndex = mapping.page_Id;

          if (bRandomTweak) {
            palRequest.ioFlag.reset();
            palRequest.ioFlag.set(idx);
          }
          else {
            palRequest.ioFlag.set();
          }

          auto block = blocks.find(palRequest.blockIndex);
          
              auto it = std::find(erased_block_id.begin(), erased_block_id.end(), palRequest.blockIndex);
              if (it != erased_block_id.end()) {

              ftlStats.inconsistantRequests++;
                return;

                            } 

          

          if (block == blocks.end()) {
            panic("Block is not in use");
          }

          beginAt = tick;

          block->second.read(palRequest.pageIndex, idx, beginAt);//here is the actual block is getting called..
          pPAL->read(palRequest, beginAt);

          finishedAt = MAX(finishedAt, beginAt);
  
            }
      }
    }
  }
 
  else{
           auto shadowIter = shadowwCache.find(req.lpn);
      if(useDeadOnArrivalPredictor && shadowIter != shadowwCache.end() && enableCMT)
      {
            cmtStats.misPredictionCount++;
             auto checkEvicted= evictedCmtEntries.find(req.lpn);
            if(checkEvicted !=evictedCmtEntries.end())
            {
              checkEvicted->second=0; // peneli
            }
            evictEntryFromCMT();
            cmt[req.lpn].block_id=shadowIter->second.block_id;
            cmt[req.lpn].page_Id=shadowIter->second.page_Id;
            cmt[req.lpn].lastAccessedTime=tick;
            cmt[req.lpn].cmtEntryAccessCount=1;
            cmtStats.entriesLoadedInCmt++;
                  
           auto cmtEntry = cmt.find(req.lpn); 
           if(cmtEntry != cmt.end())
           {
            if (bRandomTweak) {
      pDRAM->read(&(*cmtEntry), 8 * req.ioFlag.count(), tick);
    }
    else {
      pDRAM->read(&(*cmtEntry), 8, tick);
    }
      
    for (uint32_t idx = 0; idx < bitsetSize; idx++) {
      if (req.ioFlag.test(idx) || !bRandomTweak) {
        auto &mapping = cmtEntry->second;

        if (mapping.block_id < param.totalPhysicalBlocks &&
            mapping.page_Id < param.pagesInBlock) {
          palRequest.blockIndex = mapping.block_id;
          palRequest.pageIndex = mapping.page_Id;

          if (bRandomTweak) {
            palRequest.ioFlag.reset();
            palRequest.ioFlag.set(idx);
          }
          else {
            palRequest.ioFlag.set();
          }

          auto block = blocks.find(palRequest.blockIndex);
          
              auto it = std::find(erased_block_id.begin(), erased_block_id.end(), palRequest.blockIndex);
              if (it != erased_block_id.end()) {

              ftlStats.inconsistantRequests++;
                return;

                            } 

          

          if (block == blocks.end()) {
            panic("Block is not in use");
          }

          beginAt = tick;

          block->second.read(palRequest.pageIndex, idx, beginAt);//here is the actual block is getting called..
          pPAL->read(palRequest, beginAt);

          finishedAt = MAX(finishedAt, beginAt);
  
            }
      }
    }
      }
      }
      else{

  auto mappingList =
      table.find(req.lpn);  // if the mapping is present in the mapping table
      
  if (mappingList != table.end()) {  // if the mapping is present in the dram cache
                ftlStats.pageMappingFoundForReads++;
 //pageisPresentInSSD=true;
    if (bRandomTweak) {
      pDRAM->read(&(*mappingList), 8 * req.ioFlag.count(), tick);
    }
    else {
      pDRAM->read(&(*mappingList), 8, tick);
    }
           
    for (uint32_t idx = 0; idx < bitsetSize; idx++) {
      if (req.ioFlag.test(idx) || !bRandomTweak) {
        auto &mapping = mappingList->second.at(idx);

        if (mapping.first < param.totalPhysicalBlocks &&
            mapping.second < param.pagesInBlock) {
          palRequest.blockIndex = mapping.first;
          palRequest.pageIndex = mapping.second;

          if (bRandomTweak) {
            palRequest.ioFlag.reset();
            palRequest.ioFlag.set(idx);
          }
          else {
            palRequest.ioFlag.set();
          }
          if(enableCMT)
          {
           auto checkEvicted= evictedCmtEntries.find(req.lpn);// history table is checked for the entry
            if(checkEvicted !=evictedCmtEntries.end())
            {
              cmtStats.cmtEntriesEvictedAgainRequested++;
            // evictedEntryPreviousValue=checkEvicted->second;
              if(checkEvicted->second >= predictorThreshold && useDeadOnArrivalPredictor)
              {
                cmtStats.deadOnArrivalCounter++;
                deadonArrival=true;
              }
            }
            if(!deadonArrival)
            {
            //evictEntryFromCMT();
            if(optimalreplacementPolicy)
            {
                optimalRepalcementPolicy();
            }    
            else{
              //displayCMt();
                evictEntryFromCMT();
            }
            cmt[req.lpn].block_id=mapping.first;
            cmt[req.lpn].page_Id=mapping.second;
            cmt[req.lpn].lastAccessedTime=beginAt;
          //  cmt[req.lpn].cmtEntryAccessCount += (1 + evictedEntryPreviousValue);
                    cmtStats.entriesLoadedInCmt++;
                    cmt[req.lpn].cmtEntryAccessCount=1;;
                    cmt[req.lpn].iclEntry= iclCount;

                    // yeha ho skta hai SSD Read

            }
            else
            { // bypassing on the deadonArrival
              EvictFromShadowCache();
              
              shadowwCache[req.lpn].lastAccessedTime=beginAt;
               shadowwCache[req.lpn].block_id=mapping.first;
                shadowwCache[req.lpn].page_Id=mapping.second;
            }
            }
          auto block = blocks.find(palRequest.blockIndex);
          
              auto it = std::find(erased_block_id.begin(), erased_block_id.end(), palRequest.blockIndex);
              if (it != erased_block_id.end()) {
              //printf("request for deletedblock %u\n", palRequest.blockIndex);
              ftlStats.inconsistantRequests++;
                return;
          } 

          

          if (block == blocks.end()) {
            //printf("ValidPages Upper %lu superpages1 %lu superPages %lu gcCoutner %d m1 %lu l2 %lu\n\n",stat.totalValidPagesMovement,stat.validSuperPageCopies,stat.validPageCopies,gcCounter,stat.entreisPresentInmappingtableNotupdated,stat.entriesbelongingTothisblock);
            panic("Block is not in use");
          }

          beginAt = tick;

          block->second.read(palRequest.pageIndex, idx, beginAt);//here is the actual block is getting called..
          pPAL->read(palRequest, beginAt);

          finishedAt = MAX(finishedAt, beginAt);
        }
      }
    }

    tick = finishedAt;
    tick += applyLatency(CPU::FTL__PAGE_MAPPING, CPU::READ_INTERNAL);
  }
  }
}
  //printBlockState();
}////

void PageMapping::writeInternal(// this is the write after the eviction from the cache..
    Request &req, uint64_t &tick,
    bool sendToPAL) { 
     
  PAL::Request palRequest(req);
  std::unordered_map<uint32_t, Block>::iterator block;
  bool deadonArrival=false;
 uint64_t beginAt;
  uint64_t finishedAt = tick;
  bool readBeforeWrite = false;
  bool cmtWriteHit= false;
 //uint64_t evictedEntryPreviousValue=0;
   // mappinglist is the iterator of the table.
          if(sendToPAL && enableCMT) 
          {
                  
                            cmtStats.cmtWriteRequests++;
          }
   auto mappingList = table.begin();
   auto cmtEntry= cmt.find(req.lpn);
   if(cmtEntry != cmt.end() && enableCMT)
   {
     if(sendToPAL) 
          {
            cmtStats.cmtWriteRequestsHits++;
           cmtEntry->second.cmtEntryAccessCount++;
           cmtWriteHit=true;
            cmtEntry->second.lastAccessedTime=tick;
            cmtEntry->second.iclEntry=iclCount;

          }
    for (uint32_t idx = 0; idx < bitsetSize; idx++) {
      ftlStats.pageMappingFoundForWrites++;
     
      if (req.ioFlag.test(idx) || !bRandomTweak) {
        auto &mapping = cmtEntry->second;
         
        if (mapping.block_id < param.totalPhysicalBlocks &&  //blockid and 
            mapping.page_Id < param.pagesInBlock) { // page id
           // printf("Finding mapping %u\n",mapping.first);
          block = blocks.find(mapping.block_id);
          if(block !=blocks.end())
          {
          
        
          
          block->second.invalidate(mapping.page_Id, idx);
          }
        }
      }
      
    }

   
 
 }
 else if(useDeadOnArrivalPredictor && (shadowwCache.find(req.lpn) != shadowwCache.end()) && enableCMT) // agr write k time pe shadowcache ma hai
 {
  if(sendToPAL)
  {
             cmtStats.misPredictionCount++;
             auto shadowIter = shadowwCache.find(req.lpn);
             auto checkEvicted= evictedCmtEntries.find(req.lpn);
            if(checkEvicted !=evictedCmtEntries.end())
            {
              checkEvicted->second=0; // peneli
            }
            evictEntryFromCMT();
           //cmtStats.cmtWriteRequestsHits++;

            cmt[req.lpn].block_id=shadowIter->second.block_id;
            cmt[req.lpn].page_Id=shadowIter->second.page_Id;
            cmt[req.lpn].lastAccessedTime=tick;
            cmt[req.lpn].cmtEntryAccessCount=1;
            cmtStats.entriesLoadedInCmt++;
             cmtWriteHit=true;
                  
  }
  auto cmtEntry= cmt.find(req.lpn);
   if(cmtEntry != cmt.end())
   {
    
    for (uint32_t idx = 0; idx < bitsetSize; idx++) {
      ftlStats.pageMappingFoundForWrites++;
     
      if (req.ioFlag.test(idx) || !bRandomTweak) {
        auto &mapping = cmtEntry->second;
         
        if (mapping.block_id < param.totalPhysicalBlocks &&  //blockid and 
            mapping.page_Id < param.pagesInBlock) { // page id
           // printf("Finding mapping %u\n",mapping.first);
          block = blocks.find(mapping.block_id);
          if(block !=blocks.end())
          {
          
        
          
          block->second.invalidate(mapping.page_Id, idx);
          }
        }
      }
      
    }

   
 
 }

 } 
 else if (table.find(req.lpn)!=table.end()) {// agr entry mil gayi
  mappingList =table.find(req.lpn); 
    for (uint32_t idx = 0; idx < bitsetSize; idx++) {
      ftlStats.pageMappingFoundForWrites++;
      if (req.ioFlag.test(idx) || !bRandomTweak) {
        auto &mapping = mappingList->second.at(idx);

        if (mapping.first < param.totalPhysicalBlocks &&  //blockid and 
            mapping.second < param.pagesInBlock) { // page id
           // printf("Finding mapping %u\n",mapping.first);
          block = blocks.find(mapping.first);
          if(block !=blocks.end())
          {
          
        
         
          block->second.invalidate(mapping.second, idx);
          }
        }
      }
      
    }
    //printf("That is ok\n");
  }
    
  else { //agr mapping nai mili tw new entry banaw table ma

    auto ret = table.emplace(req.lpn,std::vector<std::pair<uint32_t, uint32_t>>(bitsetSize, {param.totalPhysicalBlocks, param.pagesInBlock}));



    if (!ret.second) {
      panic("Failed to insert new mapping");
    }

    mappingList = ret.first;
    //printf("Entry is %lu BlockI id %u and pageId is %u\n",mappingList->first)
  }
      block = blocks.find(getLastFreeBlock(req.ioFlag));  
    // block = blocks.find(getUpdatedLastFreeBlock());
      //printf(" Upper blockId %u ",block->second.getBlockIndex());
  if (block == blocks.end()) {
    panic("No such block");
  }

  if (sendToPAL) {
    if (bRandomTweak) {
      pDRAM->read(&(*mappingList), 8 * req.ioFlag.count(), tick);
      pDRAM->write(&(*mappingList), 8 * req.ioFlag.count(), tick);
    }
    else {
      pDRAM->read(&(*mappingList), 8, tick);
      pDRAM->write(&(*mappingList), 8, tick);
    }
  }

  if (!bRandomTweak && !req.ioFlag.all()) {
   
    readBeforeWrite = true;
  }
  for (uint32_t idx = 0; idx < bitsetSize; idx++) {
    if (req.ioFlag.test(idx) || !bRandomTweak) {
      uint32_t pageIndex = block->second.getNextWritePageIndex(idx);//get the next page to be written
      auto &mapping = mappingList->second.at(idx);// update the mapping table

      beginAt = tick;
     //printf("block %u Page %u LPN %lu Tick %lu\n", pageIndex,block->second.getBlockIndex(),req.lpn,tick);
      block->second.write(pageIndex, req.lpn, idx, beginAt);
      if (readBeforeWrite && sendToPAL) {
        palRequest.blockIndex = mapping.first;
        palRequest.pageIndex = mapping.second;

        // We don't need to read old data
        palRequest.ioFlag = req.ioFlag;
        palRequest.ioFlag.flip();
        //printf("ToCacheIn %u and %u LPN %lu\n",palRequest.blockIndex,palRequest.pageIndex,req.lpn);
        pPAL->read(palRequest, beginAt);
      }

      // update mapping to table
      mapping.first = block->first;// here the mapping is getting updated.
      mapping.second = pageIndex;
      
            
            


      if (sendToPAL) {
        if(enableCMT)
        {
        auto checkEvicted= evictedCmtEntries.find(req.lpn);
                  if(checkEvicted !=evictedCmtEntries.end())
                  {
                    cmtStats.cmtEntriesEvictedAgainRequested++;
                  // evictedEntryPreviousValue=checkEvicted->second;
                    if(checkEvicted->second >= predictorThreshold && useDeadOnArrivalPredictor)
                    {
                      cmtStats.deadOnArrivalCounter++;
                      deadonArrival=true;
                    }
                  }
        
        if(!deadonArrival)
        {     
          if(optimalreplacementPolicy)
            {
                optimalRepalcementPolicy();
            }    
            else{
             // displayCMt();
                evictEntryFromCMT();
            }
            cmt[req.lpn].block_id=mapping.first;
            cmt[req.lpn].page_Id=mapping.second;
            cmt[req.lpn].lastAccessedTime=beginAt;
           //cmt[req.lpn].cmtEntryAccessCount += (1 + evictedEntryPreviousValue);
           if(!cmtWriteHit)
           {
            cmt[req.lpn].cmtEntryAccessCount=1;
            cmt[req.lpn].iclEntry=iclCount;
            cmtStats.entriesLoadedInCmt++;
           }

            cmtWriteHit=false;
      }
       else
            { // bypassing on the deadonArrival
              EvictFromShadowCache();
              
              shadowwCache[req.lpn].lastAccessedTime=beginAt;
               shadowwCache[req.lpn].block_id=mapping.first;
                shadowwCache[req.lpn].page_Id=mapping.second;
            }
      }// end of cmt Enabling
        palRequest.blockIndex = block->first;
        palRequest.pageIndex = pageIndex;

        if (bRandomTweak) {
          palRequest.ioFlag.reset();
          palRequest.ioFlag.set(idx);
        }
        else {
          palRequest.ioFlag.set();
        }
            
        pPAL->write(palRequest, beginAt);
      }
    
      finishedAt = MAX(finishedAt, beginAt);
    }
  }
 

  // Exclude CPU operation when initializing
  if (sendToPAL) {
    tick = finishedAt;
    tick += applyLatency(CPU::FTL__PAGE_MAPPING, CPU::WRITE_INTERNAL);
  }

  // GC if needed
  // I assumed that init procedure never invokes GC
  static float gcThreshold = conf.readFloat(CONFIG_FTL, FTL_GC_THRESHOLD_RATIO);


  if (freeBlockRatio() < gcThreshold) {
    if (!sendToPAL) {
      panic("ftl: GC triggered while in initialization");
    }

    std::vector<uint32_t> list;
    uint64_t beginAt = tick;

    selectVictimBlock(list, beginAt);

    debugprint(LOG_FTL_PAGE_MAPPING,
               "GC   | On-demand | %u blocks will be reclaimed", list.size());
      // uint64_t startTime = beginAt;

        // Call doGarbageCollection
        
    doGarbageCollection(list, beginAt);

        // Measure the beginAt after doGarbageCollection
       // uint64_t endTime = beginAt;;

        // Calculate the execution time
      //  uint64_t executionTime = endTime - startTime;

        // Update the execution time counter
      //  gcTimmingBucket[executionTime]++;


    
    debugprint(LOG_FTL_PAGE_MAPPING,
               "GC   | Done | %" PRIu64 " - %" PRIu64 " (%" PRIu64 ")", tick,
               beginAt, beginAt - tick);

    stat.gcCount++;
    stat.reclaimedBlocks += list.size();
    ftlStats.FreeBlockRatioValue=freeBlockRatio();
  }
  //printBlockState();
}
void PageMapping::printBlockState()
{
for ( auto& entry : blocks) {
  uint32_t acc=entry.second.getBlockAccessCount();
  if(acc)
  {
      printf("AccessCount %u\n",acc);
  }
    }

}
uint64_t PageMapping::getBlockCachedPageCount()
{
  uint64_t cachedpages=0;
  for ( auto& entry : blocks) {
         cachedpages += entry.second.getCachedPageCount();
    }
    return cachedpages;
}
void PageMapping :: displayCMt()
{
    for (const auto& entry : cmt) {
        std::cout << "Key: " << entry.first
                  << " Block ID: " << entry.second.block_id
                  << " Page ID: " << entry.second.page_Id
                  << " Last Accessed Time: " << entry.second.lastAccessedTime
                  << " CMT Entry Access Count: " << entry.second.cmtEntryAccessCount
                  << " ICL Entry: " << entry.second.iclEntry
                  << std::endl;
    }

}
void PageMapping::writeEvictedLinesToStorage(Request & req, uint64_t & tick,std::vector<PAL::Request> &writeRequests)
{
  PAL::Request palRequest(req);
  std::unordered_map<uint32_t, Block>::iterator block;
   Bitset bit(param.ioUnitInPage);;
  auto mappingList =
      table.find(req.lpn);  // mappinglist is the iterator of the table.
  //uint64_t beginAt;
  //uint64_t finishedAt = tick;
  //bool readBeforeWrite = false;
  if (mappingList != table.end()) {// agr entry mil gayi
    //printf("In write Mapping found for the LPN %lu\n",req.lpn);
    for (uint32_t idx = 0; idx < bitsetSize; idx++) {
      pDRAM->read(&(*mappingList), 8 * param.ioUnitInPage, tick);
      if (req.ioFlag.test(idx) || !bRandomTweak) {
        auto &mapping = mappingList->second.at(idx);
         bit.set();// set the bit as written in the old.
        if (mapping.first < param.totalPhysicalBlocks &&  //blockid and 
            mapping.second < param.pagesInBlock) { // page id
           // printf("Finding mapping %u\n",mapping.first);
          block = blocks.find(mapping.first);
          if(block !=blocks.end())
          {
          
        
          if(block->second.isCached(mapping.second))
          {
          overwrittenPages++;
                     //printf("till here are ok %u and %u and cached pages %u\n",mapping.first,mapping.second,block->second.getCachedPageCount());

          block->second.test_decache(mapping.second);
          //printf("PAge Decached\n");
          }
          block->second.invalidate(mapping.second, idx);
          }
        }
              auto freeBlock = blocks.find(getLastFreeBlock(bit));
               uint32_t newBlockIdx = freeBlock->first;
              uint32_t newPageIdx = freeBlock->second.getNextWritePageIndex(idx);
              mapping.first = newBlockIdx;
            mapping.second = newPageIdx;
            freeBlock->second.write(newPageIdx, req.lpn, idx, tick);
            palRequest.blockIndex = newBlockIdx;
            palRequest.pageIndex = newPageIdx;

            if (bRandomTweak) {
              req.ioFlag.reset();
              req.ioFlag.set(idx);
            }
            else {
              req.ioFlag.set();
            }

            writeRequests.push_back(palRequest);
      }// we have invalidated the old page.
      
    }
    //printf("That is ok\n");
  }// if matching is not found meaning that the evictiLine is the write fromt he host..
  else
  {

     }
     

}
void PageMapping::writeEvictedDataToFlash(uint64_t &tick){
  printf("Value of tick %lu\n",tick);







}
void PageMapping::printEvictLines()
{
  printf("Contents of evictLIne is :-----\n");
  if(evictLines.size()==0)
  {
    printf("Buffer Empty\n");
    return;
  }
  for (const auto& line : evictLines) {
        std::cout << "tag: " << line.tag << ", dirty: " << line.dirty << ", valid: " << line.valid <<",Last Accessed"<<line.lastAccessed<< std::endl;
        // Replace the above line with the appropriate member variables you want to print
    }


}


void PageMapping::removeEntriesFromEvictionCache()
{
  //printf("Called the evictLines removal\n");
  for (const auto& line : linesToRemove) {
        // Find the matching line in 'evictLines' using the custom comparison function
        auto it = std::find_if(evictLines.begin(), evictLines.end(), [&](const SimpleSSD::ICL::_Line& l) {
            return isLineEqual(l, line);
        });

        if (it != evictLines.end()) {
         tempo--;
            evictLines.erase(it);
        }
    }


linesToRemove.clear();
}
bool PageMapping::isLineEqual(const SimpleSSD::ICL::_Line& line1, const SimpleSSD::ICL::_Line& line2)
{
  return line1.tag == line2.tag;
}
void PageMapping::newFTLRequest(uint64_t lca, uint64_t &tick)
{
 SimpleSSD::FTL::Request reqInternal(liner);
 //SimpleSSD::FTL::Request reqInternal;
  uint64_t beginAt;
  uint64_t finishedAt = tick;
  printf("Modified Eviction called for %lu\n",lca);
  debugprint(LOG_ICL_GENERIC_CACHE, "----- | Begin eviction");
  
      beginAt = tick;
        reqInternal.lpn = lca;
        reqInternal.ioFlag.reset();
        reqInternal.ioFlag.set(0);// modified evictions needed to be performed
       // printf("Eviction is callied only this number of times %u\n",col);
      //printf()
        pFTL->write(reqInternal, beginAt);// this is the write operation to the nand flash
        
finishedAt = MAX(finishedAt, beginAt);
//tick=finishedAt;
}
void PageMapping::trimInternal(Request &req, uint64_t &tick) {
  auto mappingList = table.find(req.lpn);

  if (mappingList != table.end()) {
    if (bRandomTweak) {
      pDRAM->read(&(*mappingList), 8 * req.ioFlag.count(), tick);
    }
    else {
      pDRAM->read(&(*mappingList), 8, tick);
    }

    // Do trim
    for (uint32_t idx = 0; idx < bitsetSize; idx++) {
      auto &mapping = mappingList->second.at(idx);
      auto block = blocks.find(mapping.first);

      if (block == blocks.end()) {
        panic("Block is not in use");
      }

      block->second.invalidate(mapping.second, idx);
    }

    // Remove mapping
    table.erase(mappingList);

    tick += applyLatency(CPU::FTL__PAGE_MAPPING, CPU::TRIM_INTERNAL);
  }
}

void PageMapping::eraseInternal(PAL::Request &req, uint64_t &tick) {
  static uint64_t threshold =
      conf.readUint(CONFIG_FTL, FTL_BAD_BLOCK_THRESHOLD);
      static const EVICT_POLICY policy =
      (EVICT_POLICY)conf.readInt(CONFIG_FTL, FTL_GC_EVICT_POLICY);
  auto block = blocks.find(req.blockIndex);

  // Sanity checks
  if (block == blocks.end()) {
    panic("No such block");
  }
if((policy != CACHED_GC) && (policy != HOTBLOCK))// this things is changed in the cache..
{
  if (block->second.getValidPageCount() != 0) {
   // panic("There are valid pages in victim block");
  }
}

  // Erase block
  block->second.erase();

  pPAL->erase(req, tick);

  // Check erase count
  uint32_t erasedCount = block->second.getEraseCount();

  if (erasedCount < threshold) {
    // Reverse search
    auto iter = freeBlocks.end();

    while (true) {
      iter--;

      if (iter->getEraseCount() <= erasedCount) {
        // emplace: insert before pos
        iter++;

        break;
      }

      if (iter == freeBlocks.begin()) {
        break;
      }
    }

    // Insert block to free block list
    freeBlocks.emplace(iter, std::move(block->second));
    //
    nFreeBlocks++;
   // printf("After Erase FreeBlockRatio %f\n",freeBlockRatio());
  }

  blocks.erase(block);
 

  tick += applyLatency(CPU::FTL__PAGE_MAPPING, CPU::ERASE_INTERNAL);
}
void PageMapping::RemoveEntryFromTable(uint64_t key_to_remove,uint64_t &tick) {
  auto it = table.find(key_to_remove);
    debugprint(LOG_ICL_GENERIC_CACHE, "EntryUpdated  | Tick %" PRIu64, tick);

  pDRAM->write(&(*it), 8 * param.ioUnitInPage, tick);
      if (it != table.end()) {
        table.erase(it);

    } 
}

float PageMapping::calculateWearLeveling() {
  uint64_t totalEraseCnt = 0;
  uint64_t sumOfSquaredEraseCnt = 0;
  uint64_t numOfBlocks = param.totalLogicalBlocks;
  uint64_t eraseCnt;

  for (auto &iter : blocks) {
    eraseCnt = iter.second.getEraseCount();
    totalEraseCnt += eraseCnt;
    sumOfSquaredEraseCnt += eraseCnt * eraseCnt;
  }

  // freeBlocks is sorted
  // Calculate from backward, stop when eraseCnt is zero
  for (auto riter = freeBlocks.rbegin(); riter != freeBlocks.rend(); riter++) {
    eraseCnt = riter->getEraseCount();

    if (eraseCnt == 0) {
      break;
    }

    totalEraseCnt += eraseCnt;
    sumOfSquaredEraseCnt += eraseCnt * eraseCnt;
  }

  if (sumOfSquaredEraseCnt == 0) {
    return -1;  // no meaning of wear-leveling
  }

  return (float)totalEraseCnt * totalEraseCnt /
         (numOfBlocks * sumOfSquaredEraseCnt);
}

void PageMapping::calculateTotalPages(uint64_t &valid, uint64_t &invalid) {
  valid = 0;
  invalid = 0;

  for (auto &iter : blocks) {
    valid += iter.second.getValidPageCount();
    invalid += iter.second.getDirtyPageCount();
  }
}

void PageMapping::getStatList(std::vector<Stats> &list, std::string prefix) {
  Stats temp;

  temp.name = prefix + "page_mapping.gc.count";
  temp.desc = "Total GC count";
  list.push_back(temp);

  temp.name = prefix + "page_mapping.gc.reclaimed_blocks";
  temp.desc = "Total reclaimed blocks in GC";
  list.push_back(temp);

  temp.name = prefix + "page_mapping.gc.superpage_copies";
  temp.desc = "Total copied valid superpages during GC";
  list.push_back(temp);

  temp.name = prefix + "page_mapping.gc.page_copies";
  temp.desc = "Total copied valid pages during GC";
  list.push_back(temp);

  // For the exact definition, see following paper:
  // Li, Yongkun, Patrick PC Lee, and John Lui.
  // "Stochastic modeling of large-scale solid-state storage systems: analysis,
  // design tradeoffs and optimization." ACM SIGMETRICS (2013)
  temp.name = prefix + "page_mapping.wear_leveling";
  temp.desc = "Wear-leveling factor";
  list.push_back(temp);
}

void PageMapping:: printFtlStats()
{
   
    printf("TotalFtlReadRequests: %lu\n", ftlStats.TotalFtlReadRequests);
    printf("TotalFtlWriteRequests: %lu\n", ftlStats.TotalFtlWriteRequests);
    printf("pageMappingFoundForReads: %lu\n", ftlStats.pageMappingFoundForReads);
    printf("pageMappingFoundForWrites: %lu\n", ftlStats.pageMappingFoundForWrites);
    printf("FreeBlockRationAtEnd: %f\n",ftlStats.FreeBlockRatioValue);
    printf("BlockToBlockMovement %lu\n",stat.victimToFreeMovements);
    printf("CachedPageCount %lu\n",stat.cachedPageCount);
    printf("PagesValidButNotCached %lu\n",stat.pagesNotCachedButValid);
    printf("PagesUpdatedDirty %lu\n",stat.pagesUpdatedDirty);
    printf("totalValidPageMovement %lu\n",stat.totalValidPagesMovement);
    printf("cachedpagesfoundInAllBlocks %u\n",ftlStats.cachedpagesfoundInAllBlocks);
    printf("cmtReadRequests %lu\n",cmtStats.cmtReadRequests);
    printf("cmtReadRequestsHits %lu\n",cmtStats.cmtReadRequestsHits);
    printf("cmtWriteRequests %lu\n",cmtStats.cmtWriteRequests);
    printf("cmtWriteRequestsHits %lu\n",cmtStats.cmtWriteRequestsHits);
    printf("cmtEntriesEvictedAgainRequested %lu\n", cmtStats.cmtEntriesEvictedAgainRequested);
    printf("deadOnArrivalCounter %lu\n",cmtStats.deadOnArrivalCounter);
    printf("EvictionsFromCmt %lu\n",cmtStats.cmtEvictions);
    printf("EntriesEvictedWithOneAccessCount %lu\n",cmtStats.entriesWithOneAccessCount);
    printf("Inconsistant requests %lu\n",ftlStats.inconsistantRequests);
    printf("misPredictionCount %lu \n",cmtStats.misPredictionCount);
    printf("ShadowCacheEviction %lu\n",cmtStats.EvictionsFromShadowCache);
    printf("CmtEntresLoadedInCmt %lu\n",cmtStats.entriesLoadedInCmt);
    if(printDeadOnArrival)
    {
    printf("DeadPagePercentage \n");
    for(auto i: DeadPagePercentage)
    {
      printf("%f \t",i);
    }
    printf("\n");
    }



}
void PageMapping::getStatValues(std::vector<double> &values) {
  values.push_back(stat.gcCount);
  values.push_back(stat.reclaimedBlocks);
  values.push_back(stat.validSuperPageCopies);
  values.push_back(stat.validPageCopies);
  values.push_back(calculateWearLeveling());
}

void PageMapping::resetStatValues() {
  memset(&stat, 0, sizeof(stat));
  memset(&ftlStats, 0, sizeof(ftlStats));
   memset(&cmtStats, 0, sizeof(cmtStats));
  
   

}

}  // namespace FTL

}  // namespace SimpleSSD
