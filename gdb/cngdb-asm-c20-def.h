static struct c20_instruction c20_instr[] = {
  /* scalar inst */
  {
    cngdb_asm_c20_cb,
    {
      {Finish, 511, 511, 0},
      {ConstData, 510, 503, 11},
      {LayerType, 502, 495, 0},
      {RESERVED, 494, 490, 0},
      {AddrRegEn, 489, 489, 0},
      {AddrRegId, 488, 484, 0},
      {AddrImme, 483, 468, 0},
      {ChangePosEn, 467, 467, 0},
      {OperatorType, 466, 463, 0},
      {Src1RegId, 462, 458, 0},
      {Src2RegEn, 457, 457, 0},
      {Src2RegId, 456, 452, 0},
      {Src2Imme, 451, 388, 0},
      {AddrImmeH, 387, 356, 0},
      {SyncDMA0En, 355, 355, 0},
      {SyncDMA1En, 354, 354, 0},
      {SyncScalarEn, 353, 353, 0},
      {SyncStreamEn, 352, 352, 0},
      {SyncRobEn, 351, 351, 0},
      {RESERVED, 350, 5, 0},
      {Predicate, 4, 0, 0},
    }
  },
  {
    cngdb_asm_c20_jump_prefetch,
    {
      {Finish, 511, 511, 0},
      {ConstData, 510, 503, 14},
      {AddrRegId, 502, 498, 0},
      {AddrRegEn, 497, 497, 0},
      {LinkRegEn, 496, 496, 0},
      {AddrImmeL, 495, 480, 0},
      {AddrImmeH, 479, 448, 0},
      {PrefetchLegthRegId, 447, 443, 0},
      {PrefetchLegthRegEn, 442, 442, 0},
      {PrefetchLegthRegImm, 441, 431, 0},
      {SyncDMA0En, 430, 430, 0},
      {SyncDMA1En, 429, 429, 0},
      {SyncScalarEn, 428, 428, 0},
      {SyncStreamEn, 427, 427, 0},
      {SyncRobEn, 426, 426, 0},
      {RESERVED, 425, 5, 0},
      {Predicate, 4, 0, 0},
    }
  },
  {
    cngdb_asm_c20_barrier,
    {
      {Finish, 511, 511, 0},
      {ConstData, 510, 503, 12},
      {BarId, 502, 439, 0},
      {BarCount, 438, 423, 1},
      {Sync, 422, 422, 0},
      {Global, 421, 421, 0},
      {BarCountRegEn, 420, 420, 0},
      {BarCountRegId, 419, 415, 0},
      {BarIdRegEn, 414, 414, 0},
      {BarIdRegId, 413, 409, 0},
      {DMA0NotSyncEn, 408, 408, 0},
      {DMA1NotSyncEn, 407, 407, 0},
      {ScalarNotSyncEn, 406, 406, 0},
      {StreamNotSyncEn, 405, 405, 0},
      {RobNotSyncEn, 404, 404, 0},
      {RESERVED, 403, 5, 0},
      {Predicate, 4, 0, 0},
    }
  },
  /* finish inst, mark instruction end */
  {
    NULL,
    {
    }
  },
};

