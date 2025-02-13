// Copyright (c) 2022 Octavian Oncescu
// Copyright (c) 2022-2023 The Purplecoin Core developers
// Licensed under the Apache License, Version 2.0 see LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0 or the MIT license, see
// LICENSE-MIT or http://opensource.org/licenses/MIT

use crate::chain::mmr::leaf_set::LeafSetRwLockIter;
use crate::chain::mmr::{
    leaf_set::LeafSet, prune_list::PruneList, util::is_leaf, MMRBackend, MMRBackendErr,
};
use crate::chain::{
    BlockHeaderWithHash, ChainConfig, DBInterface, DBPrefixIterator, IteratorDirection,
    PowBlockHeaderWithHash, PowChainBackend, PowChainBackendErr, SectorConfig, ShardBackend,
    ShardBackendErr, ShardConfig,
};
use crate::consensus::SHARDS_PER_SECTOR;
use crate::primitives::{Block, BlockData, BlockHeader, Hash256, Output, PowBlock, PowBlockHeader};
use accumulator::group::{Codec, Rsa2048};
use accumulator::Witness;
use dashmap::{DashMap as HashMap, DashSet as HashSet};
use parking_lot::RwLock;
use rocksdb::Error as RocksDBErr;
use rocksdb::{Direction, IteratorMode, MultiThreaded, TransactionDB, WriteBatchWithTransaction};
use std::borrow::Borrow;
use std::cmp;
use std::convert::Into;
use std::sync::atomic::AtomicU64;
use streaming_iterator::StreamingIterator;
use triomphe::Arc;

pub type DB = TransactionDB<MultiThreaded>;
pub type WriteBatch = WriteBatchWithTransaction<true>;

pub const SECTOR_HEADERS_CF: &str = "sector_headers";
pub const SHARD_HEADERS_CF: &str = "shard_headers";
pub const MMR_CF: &str = "mmr";
pub const TRANSACTIONS_CF: &str = "transactions";
pub const OUTPUTS_CF: &str = "outputs";
pub const SHARE_CHAIN_CF: &str = "sharechain";

#[derive(Clone)]
pub struct DiskBackend {
    shard_config: Option<ShardConfig>,
    sector_config: Option<SectorConfig>,
    chain_config: Arc<ChainConfig>,
    db: Arc<DB>,
    prune_list: Option<Arc<RwLock<PruneList>>>,
    leaf_set: Option<Arc<RwLock<LeafSet>>>,
    cached_height: Arc<AtomicU64>,
    orphan_pool: Arc<HashSet<Hash256>>,
    block_buf: Arc<HashMap<Hash256, BlockHeader>>,
}

impl DiskBackend {
    pub fn new(
        db: Arc<DB>,
        chain_config: Arc<ChainConfig>,
        shard_config: Option<ShardConfig>,
        sector_config: Option<SectorConfig>,
        prune_list: Option<Arc<RwLock<PruneList>>>,
        leaf_set: Option<Arc<RwLock<LeafSet>>>,
    ) -> Result<Self, ShardBackendErr> {
        Ok(Self {
            db: db.clone(),
            chain_config,
            shard_config,
            sector_config,
            prune_list,
            leaf_set,
            cached_height: Arc::new(AtomicU64::new(0)),
            orphan_pool: Arc::new(HashSet::new()),
            block_buf: Arc::new(HashMap::new()),
        })
    }

    #[must_use]
    pub fn is_shard(&self) -> bool {
        self.shard_config.is_some()
    }

    #[must_use]
    pub fn is_sector(&self) -> bool {
        self.sector_config.is_some()
    }

    pub fn get_cf(&self, cf: &str, k: &[u8]) -> Result<Option<Vec<u8>>, RocksDBErr> {
        unimplemented!();
    }

    pub fn put_cf(&self, cf: &str, k: Vec<u8>, v: Vec<u8>) -> Result<Option<Vec<u8>>, RocksDBErr> {
        unimplemented!();
    }
}

impl DBInterface for DiskBackend {
    fn get<K: AsRef<[u8]>, V: bincode::Decode>(
        &self,
        key: K,
    ) -> Result<Option<V>, super::DBInterfaceErr> {
        let result = self.db.get(key)?;
        Ok(result.map(|r| crate::codec::decode::<V>(&r).expect("db corruption")))
    }

    fn put<K: AsRef<[u8]>, V: bincode::Encode>(
        &self,
        key: K,
        v: V,
    ) -> Result<(), super::DBInterfaceErr> {
        Ok(self.db.put(key, crate::codec::encode_to_vec(&v).unwrap())?)
    }

    fn delete<K: AsRef<[u8]>, V: bincode::Decode>(
        &self,
        key: K,
    ) -> Result<(), super::DBInterfaceErr> {
        Ok(self.db.delete(key)?)
    }

    fn prefix_iterator<'b, V: bincode::Decode + 'b>(
        &self,
        prefix: Vec<u8>,
        direction: IteratorDirection,
    ) -> Box<dyn StreamingIterator<Item = (Vec<u8>, V)> + 'b> {
        let data = self
            .db
            .iterator(IteratorMode::From(prefix.as_slice(), Direction::Forward))
            .map(|r| {
                let (k, v) = r.expect("db error");
                (
                    k.as_ref().to_vec(),
                    crate::codec::decode(v.as_ref()).unwrap(),
                )
            })
            .collect::<Vec<(Vec<u8>, V)>>();

        Box::new(DBPrefixIterator::new(data, direction))
    }

    fn db_handle(&self) -> Option<Arc<DB>> {
        Some(self.db.clone())
    }
}

impl PowChainBackend for DiskBackend {
    fn get_canonical_pow_block(
        &self,
        hash: &Hash256,
    ) -> Result<Option<PowBlockHeader>, PowChainBackendErr> {
        unimplemented!()
    }

    fn get_sector_canonical_block_hashes(
        &self,
    ) -> Result<[Option<Hash256>; 64], PowChainBackendErr> {
        unimplemented!()
    }

    fn get_sector_canonical_blocks(
        &self,
    ) -> Result<[Option<BlockHeader>; SHARDS_PER_SECTOR], PowChainBackendErr> {
        unimplemented!()
    }

    fn get_shard_canonical_block_header(
        &self,
        chain_id: u8,
    ) -> Result<BlockHeader, PowChainBackendErr> {
        unimplemented!()
    }

    fn get_shard_canonical_block_header_at_height(
        &self,
        chain_id: u8,
        height: u64,
    ) -> Result<Option<BlockHeader>, PowChainBackendErr> {
        unimplemented!()
    }

    fn get_shard_block_data(
        &self,
        chain_id: u8,
        block_hash: &Hash256,
    ) -> Result<Option<BlockData>, PowChainBackendErr> {
        unimplemented!()
    }

    fn get_canonical_pow_block_at_height(
        &self,
        pos: u64,
    ) -> Result<Option<PowBlockHeader>, PowChainBackendErr> {
        unimplemented!()
    }

    fn get_non_canonical_blocks_at_height(
        &self,
        h: u64,
    ) -> Result<Vec<PowBlockHeader>, PowChainBackendErr> {
        unimplemented!()
    }

    fn get_runnerups_at_height(&self, h: u64) -> Result<Vec<PowBlockHeader>, PowChainBackendErr> {
        unimplemented!()
    }

    fn rewind(&self, pos: u64) -> Result<(), PowChainBackendErr> {
        unimplemented!()
    }

    fn prune_headers(&self, pos: u64) -> Result<(), PowChainBackendErr> {
        unimplemented!()
    }

    fn chain_config(&self) -> &ChainConfig {
        self.chain_config.as_ref()
    }

    fn sector_config(&self) -> &SectorConfig {
        self.sector_config.as_ref().unwrap()
    }

    fn height(&self) -> Result<u64, PowChainBackendErr> {
        unimplemented!()
    }

    fn write_header(&self, block_header: &PowBlockHeader) -> Result<(), PowChainBackendErr> {
        unimplemented!()
    }

    fn write_block_batch(
        &self,
        block: &PowBlock,
        batch: &[(Block, Vec<Output>)],
    ) -> Result<(), PowChainBackendErr> {
        unimplemented!()
    }

    fn set_sector_config(&mut self, config: SectorConfig) {
        self.sector_config = Some(config);
    }

    fn set_prune_list(&mut self, prune_list: Arc<RwLock<PruneList>>) {
        self.prune_list = Some(prune_list);
    }

    fn set_leaf_set(&mut self, leaf_set: Arc<RwLock<LeafSet>>) {
        self.leaf_set = Some(leaf_set);
    }

    fn write_runnerup_header(
        &self,
        block_header: &PowBlockHeader,
    ) -> Result<(), PowChainBackendErr> {
        let header: PowBlockHeaderWithHash = block_header.clone().into();
        let mut batch = self.db.transaction();
        let headers_cf = self.db.cf_handle(SECTOR_HEADERS_CF).unwrap();
        batch.put_cf(
            &headers_cf,
            header.hash.as_bytes(),
            crate::codec::encode_to_vec(&header)?,
        );
        batch.commit()?;
        Ok(())
    }
}

impl ShardBackend for DiskBackend {
    fn rewind(&self, pos: u64) -> Result<(), ShardBackendErr> {
        if ShardBackend::height(self)? <= pos {
            return Err(ShardBackendErr::InvalidHeight);
        }

        unimplemented!()
    }

    fn write_header(&self, block_header: &BlockHeader) -> Result<(), ShardBackendErr> {
        debug_assert!(block_header.height > 1);
        let headerwh: BlockHeaderWithHash = block_header.clone().into();
        let hbytes = headerwh.header.height.to_le_bytes();
        let mut tx = self.db.transaction();
        let hkey = &["h".as_bytes(), &[block_header.chain_id]].concat();
        let bhkey = &[[block_header.chain_id].as_slice(), &hbytes].concat();
        let headers_cf = self.db.cf_handle(SHARD_HEADERS_CF).unwrap();
        let cur_height = tx.get_cf(&headers_cf, hkey)?.map_or(1, |bytes| {
            let mut v: [u8; 8] = [0; 8];
            v.copy_from_slice(&bytes);
            u64::from_le_bytes(v)
        });

        tx.put_cf(
            &headers_cf,
            headerwh.hash.as_bytes(),
            crate::codec::encode_to_vec(&headerwh)?,
        );

        // Establish new canonical chain
        if headerwh.header.height > cur_height {
            tx.put_cf(&headers_cf, hkey, hbytes);
            tx.put_cf(&headers_cf, bhkey, headerwh.hash.as_bytes());
        }
        tx.commit()?;
        Ok(())
    }

    fn write_block_data(
        &self,
        block: &Block,
        outputs: Vec<(Output, Witness<Rsa2048, Output>)>,
    ) -> Result<(), ShardBackendErr> {
        let mut batch = WriteBatch::default();
        let hash = block.header.hash().unwrap();
        let pubkeys_key = &[b"p", hash.0.as_slice()].concat();
        let sig_key = &[b"s", hash.0.as_slice()].concat();
        let tx_count_key = &[b"t", hash.0.as_slice()].concat();
        let transactions_cf = self.db.cf_handle(TRANSACTIONS_CF).unwrap();
        let outputs_cf = self.db.cf_handle(OUTPUTS_CF).unwrap();
        batch.put_cf(
            &transactions_cf,
            tx_count_key,
            (block.body.txs.len() as u16).to_le_bytes(),
        );

        for (i, tx) in block.body.txs.iter().enumerate() {
            let i = i as u16;
            let tx_idx_key = &[&i.to_le_bytes(), hash.0.as_slice()].concat();
            batch.put_cf(&transactions_cf, tx_idx_key, tx.hash().unwrap().0);
            batch.put_cf(&transactions_cf, tx.hash().unwrap().0, &tx.to_bytes());
        }

        for out in &outputs {
            batch.put_cf(
                &outputs_cf,
                out.0.hash().unwrap().0,
                &crate::codec::encode_to_vec(&(out.0.clone(), out.1.to_bytes())).unwrap(),
            );
        }

        if let Some(sig) = block.body.aggregated_signature.as_ref() {
            batch.put_cf(&transactions_cf, sig_key, sig.to_bytes());
        }

        self.db.write(batch)?;
        Ok(())
    }

    fn get_canonical_block(&self, hash: &Hash256) -> Result<Option<BlockHeader>, ShardBackendErr> {
        unimplemented!()
    }

    fn get_canonical_block_at_height(
        &self,
        h: u64,
    ) -> Result<Option<BlockHeader>, ShardBackendErr> {
        let bhkey = &[[self.shard_config().chain_id].as_slice(), &h.to_le_bytes()].concat();
        let headers_cf = self.db.cf_handle(SHARD_HEADERS_CF).unwrap();
        let hash = self.db.get_cf(&headers_cf, bhkey)?;
        if hash.is_none() {
            return Ok(None);
        }
        let hbytes = self.db.get_cf(&headers_cf, hash.as_ref().unwrap())?;
        if hbytes.is_none() {
            return Ok(None);
        }

        let headerwh: BlockHeaderWithHash =
            crate::codec::decode(&hbytes.unwrap()).map_err(|_| ShardBackendErr::CorruptData)?;
        Ok(Some(headerwh.into()))
    }

    fn get_non_canonical_blocks_at_height(
        &self,
        h: u64,
    ) -> Result<Vec<BlockHeader>, ShardBackendErr> {
        unimplemented!()
    }

    fn get_block_data(&self, hash: &Hash256) -> Result<Option<BlockData>, ShardBackendErr> {
        unimplemented!()
    }

    fn prune_headers(&self, pos: u64) -> Result<(), ShardBackendErr> {
        unimplemented!()
    }

    fn prune_utxos(&self, pos: u64) -> Result<(), ShardBackendErr> {
        unimplemented!()
    }

    // TODO: Cache this
    fn height(&self) -> Result<u64, ShardBackendErr> {
        let key = &["h".as_bytes(), &[self.shard_config().chain_id]].concat();
        let headers_cf = self.db.cf_handle(SHARD_HEADERS_CF).unwrap();
        let tx = self.db.transaction();
        match tx.get_cf(&headers_cf, key) {
            Ok(Some(bheight)) => {
                let bheight = tx
                    .get_cf(&headers_cf, key)?
                    .ok_or(ShardBackendErr::CorruptData)?;
                let mut out = [0; 8];
                out.copy_from_slice(&bheight);
                Ok(u64::from_le_bytes(out))
            }

            Ok(None) => {
                tx.put_cf(&headers_cf, key, 1_u64.to_le_bytes())?;
                tx.commit()?;
                Ok(1)
            }

            Err(err) => Err(err.into()),
        }
    }

    fn shard_config(&self) -> &ShardConfig {
        self.shard_config.as_ref().unwrap()
    }

    fn chain_config(&self) -> &ChainConfig {
        &self.chain_config
    }

    fn utxo(&self, key: &Hash256) -> Option<Output> {
        unimplemented!()
    }

    fn set_shard_config(&mut self, config: ShardConfig) {
        self.shard_config = Some(config);
    }

    fn get_val<T: AsRef<[u8]>>(&self, key: T) -> Result<Option<Vec<u8>>, ShardBackendErr> {
        unimplemented!();
    }

    fn write_key_vals_batch<T: AsRef<[u8]>>(
        &self,
        key_vals: Vec<(T, T)>,
    ) -> Result<(), ShardBackendErr> {
        unimplemented!()
    }

    fn tip_pow(&self) -> Result<PowBlockHeader, ShardBackendErr> {
        PowChainBackend::tip(self).map_err(std::convert::Into::into)
    }
}

impl MMRBackend<Vec<u8>> for DiskBackend {
    fn get(&self, pos: u64) -> Result<Option<Hash256>, MMRBackendErr> {
        unimplemented!()
    }

    fn get_leaf(&self, hash: &Hash256) -> Result<Option<Vec<u8>>, MMRBackendErr> {
        let key = &[&[self.sector_config().sector_id], hash.as_bytes()].concat();
        let mmr_cf = self.db.cf_handle(MMR_CF).unwrap();
        Ok(self.db.get_cf(&mmr_cf, key)?)
    }

    fn write_leaf(&self, hash: Hash256, leaf: &Vec<u8>) -> Result<(), MMRBackendErr> {
        let encoded = crate::codec::encode_to_vec(leaf);
        let key = &[&[self.sector_config().sector_id], hash.as_bytes()].concat();
        let mmr_cf = self.db.cf_handle(MMR_CF).unwrap();
        self.db.put_cf(&mmr_cf, key, leaf)?;
        Ok(())
    }

    fn get_peak(&self, pos: u64) -> Result<Option<Hash256>, MMRBackendErr> {
        // let shift = self.prune_list.as_ref().expect("prune list not initialised").read().get_shift(pos);
        // let pos = 1 + pos + shift;

        let key = &[
            "h".as_bytes(),
            &[self.sector_config().sector_id],
            &pos.to_be_bytes(),
        ]
        .concat();
        let mmr_cf = self.db.cf_handle(MMR_CF).unwrap();
        Ok(self.db.get_cf(&mmr_cf, key)?.map(Into::into))
    }

    fn get_hash(&self, pos: u64) -> Result<Option<Hash256>, MMRBackendErr> {
        // let shift = self.prune_list.as_ref().expect("prune list not initialised").read().get_shift(pos);
        // let pos = 1 + pos + shift;

        let key = &[
            "h".as_bytes(),
            &[self.sector_config().sector_id],
            &pos.to_be_bytes(),
        ]
        .concat();
        let mmr_cf = self.db.cf_handle(MMR_CF).unwrap();
        Ok(self.db.get_cf(&mmr_cf, key)?.map(Into::into))
    }

    fn write_hash_at_pos(&self, hash: Hash256, pos: u64) -> Result<(), MMRBackendErr> {
        let key = &[
            "h".as_bytes(),
            &[self.sector_config().sector_id],
            &pos.to_be_bytes(),
        ]
        .concat();
        let mmr_cf = self.db.cf_handle(MMR_CF).unwrap();
        self.db.put_cf(&mmr_cf, key, hash)?;

        // Write leaf position
        if is_leaf(pos) {
            let key = &[
                "l".as_bytes(),
                &[self.sector_config().sector_id],
                &self.unpruned_size()?.to_be_bytes(),
            ]
            .concat();
            self.db.put_cf(&mmr_cf, key, pos.to_le_bytes());
        }

        Ok(())
    }

    fn n_unpruned_leaves(&self) -> u64 {
        self.leaf_set
            .as_ref()
            .expect("leaf set not initialised")
            .read()
            .len() as u64
    }

    fn n_unpruned_leaves_to_index(&self, to_index: u64) -> u64 {
        self.leaf_set
            .as_ref()
            .expect("leaf set not initialised")
            .read()
            .n_unpruned_leaves_to_index(to_index)
    }

    fn unpruned_size(&self) -> Result<u64, MMRBackendErr> {
        Ok(self.size()?
            + self
                .prune_list
                .as_ref()
                .expect("leaf set not initialised")
                .read()
                .get_total_shift())
    }

    fn hash_key(&self) -> &str {
        if self.is_shard() {
            return self.shard_config.as_ref().unwrap().key();
        }

        if self.is_sector() {
            return self.sector_config.as_ref().unwrap().key();
        }

        unreachable!()
    }

    fn size(&self) -> Result<u64, MMRBackendErr> {
        let key = &["s".as_bytes(), &[self.sector_config().sector_id]].concat();
        let mmr_cf = self.db.cf_handle(MMR_CF).unwrap();
        let tx = self.db.transaction();
        match tx.get_cf(&mmr_cf, key) {
            Ok(Some(bheight)) => {
                let bheight = self
                    .db
                    .get_cf(&mmr_cf, key)?
                    .ok_or(MMRBackendErr::CorruptData)?;
                let mut out = [0; 8];
                out.copy_from_slice(&bheight);
                Ok(u64::from_le_bytes(out))
            }

            Ok(None) => {
                tx.put_cf(&mmr_cf, key, 0_u64.to_le_bytes())?;
                tx.commit()?;
                Ok(0)
            }

            Err(err) => Err(err.into()),
        }
    }

    fn set_size(&self, size: u64) -> Result<(), MMRBackendErr> {
        let key = &["s".as_bytes(), &[self.sector_config().sector_id]].concat();
        let mmr_cf = self.db.cf_handle(MMR_CF).unwrap();
        self.db.put_cf(&mmr_cf, key, size.to_le_bytes())?;
        Ok(())
    }

    fn flush(&mut self) -> Result<(), MMRBackendErr> {
        self.prune_list.as_ref().unwrap().write().flush();
        self.leaf_set.as_ref().unwrap().write().flush();
        Ok(())
    }

    fn prune(&mut self) -> Result<(), MMRBackendErr> {
        unimplemented!()
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::boxed::Box;

    // Disk backend tests don't work on the Windows CI due to temp dir
    // permissions so we must disable them via a feature flag.

    #[test]
    #[cfg(not(feature = "disable_tests_on_windows"))]
    fn mmr_push() {
        let db = crate::chain::create_rocksdb_backend();
        let mut backend = DiskBackend::new(
            db.clone(),
            Default::default(),
            None,
            Some(Default::default()),
            Some(Arc::new(RwLock::new(
                PruneList::open(Some(db.clone()), "testpl").unwrap(),
            ))),
            Some(Arc::new(RwLock::new(
                LeafSet::open(Some(db), "testls").unwrap(),
            ))),
        )
        .unwrap();
        assert_eq!(Ok(0), backend.push(&vec![0x00, 0x01]));
        assert_eq!(Ok(1), backend.push(&vec![0x01, 0x00]));
        assert_eq!(Ok(3), backend.push(&vec![0x01, 0x00, 0x00]));
        assert_eq!(Ok(4), backend.push(&vec![0x01, 0x00, 0x01]));
        assert_eq!(Ok(7), backend.push(&vec![0x01, 0x01, 0x01]));
        assert_eq!(Ok(8), backend.push(&vec![0x01, 0x01, 0x02]));
    }

    #[test]
    #[cfg(not(feature = "disable_tests_on_windows"))]
    fn mmr_write_leaf() {
        let db = crate::chain::create_rocksdb_backend();
        let mut backend = DiskBackend::new(
            db.clone(),
            Default::default(),
            Some(Default::default()),
            Some(Default::default()),
            Some(Arc::new(RwLock::new(
                PruneList::open(Some(db.clone()), "testpl").unwrap(),
            ))),
            Some(Arc::new(RwLock::new(
                LeafSet::open(Some(db), "testls").unwrap(),
            ))),
        )
        .unwrap();
        let h1 = Hash256::hash_from_slice("test1", "asdf");
        let h2 = Hash256::hash_from_slice("test2", "asdf");
        backend.write_leaf(h1, &vec![0x00, 0x01]);
        backend.write_leaf(h2, &vec![0x01, 0x00]);

        assert_eq!(backend.get_leaf(&h1), Ok(Some(vec![0x00, 0x01])));
        assert_eq!(backend.get_leaf(&h2), Ok(Some(vec![0x01, 0x00])));
    }

    #[test]
    #[cfg(not(feature = "disable_tests_on_windows"))]
    fn mmr_write_get_hash_at_pos() {
        let db = crate::chain::create_rocksdb_backend();
        let mut backend = DiskBackend::new(
            db.clone(),
            Default::default(),
            Some(Default::default()),
            Some(Default::default()),
            Some(Arc::new(RwLock::new(
                PruneList::open(Some(db.clone()), "testpl").unwrap(),
            ))),
            Some(Arc::new(RwLock::new(
                LeafSet::open(Some(db), "testls").unwrap(),
            ))),
        )
        .unwrap();
        let h1 = Hash256::hash_from_slice("test1", "asdf");
        let h2 = Hash256::hash_from_slice("test2", "asdf");
        backend.write_hash_at_pos(h1, 0);
        backend.write_hash_at_pos(h2, 1);

        assert_eq!(backend.get_hash(0), Ok(Some(h1)));
        assert_eq!(backend.get_hash(1), Ok(Some(h2)));
    }

    #[test]
    #[cfg(not(feature = "disable_tests_on_windows"))]
    fn mmr_backends_are_domain_separated_by_chain_id() {
        let db = crate::chain::create_rocksdb_backend();
        let mut backend1 = DiskBackend::new(
            db.clone(),
            Default::default(),
            Some(Default::default()),
            Some(Default::default()),
            Some(Arc::new(RwLock::new(
                PruneList::open(Some(db.clone()), "testpl").unwrap(),
            ))),
            Some(Arc::new(RwLock::new(
                LeafSet::open(Some(db.clone()), "testls").unwrap(),
            ))),
        )
        .unwrap();
        let mut backend2 = DiskBackend::new(
            db.clone(),
            Default::default(),
            Some(Default::default()),
            Some(Default::default()),
            Some(Arc::new(RwLock::new(
                PruneList::open(Some(db.clone()), "testpl").unwrap(),
            ))),
            Some(Arc::new(RwLock::new(
                LeafSet::open(Some(db.clone()), "testls").unwrap(),
            ))),
        )
        .unwrap();
        let mut backend3 = DiskBackend::new(
            db.clone(),
            Default::default(),
            Some(Default::default()),
            Some(Default::default()),
            Some(Arc::new(RwLock::new(
                PruneList::open(Some(db.clone()), "testpl2").unwrap(),
            ))),
            Some(Arc::new(RwLock::new(
                LeafSet::open(Some(db), "testls2").unwrap(),
            ))),
        )
        .unwrap();
        backend3.sector_config.as_mut().unwrap().sector_id = 1;
        let h1 = Hash256::hash_from_slice("test1", "asdf");
        let h2 = Hash256::hash_from_slice("test2", "asdf");
        backend1.write_hash_at_pos(h1, 0);
        backend1.write_hash_at_pos(h2, 1);

        assert_eq!(backend1.get_hash(0), Ok(Some(h1)));
        assert_eq!(backend1.get_hash(1), Ok(Some(h2)));
        assert_eq!(backend2.get_hash(0), Ok(Some(h1)));
        assert_eq!(backend2.get_hash(1), Ok(Some(h2)));
        assert_eq!(backend3.get_hash(0), Ok(None));
        assert_eq!(backend3.get_hash(1), Ok(None));
    }

    #[test]
    #[cfg(not(feature = "disable_tests_on_windows"))]
    fn mmr_backends_are_domain_separated_by_chain_id_write_leaf() {
        let db = crate::chain::create_rocksdb_backend();
        let mut backend1 = DiskBackend::new(
            db.clone(),
            Default::default(),
            Some(Default::default()),
            Some(Default::default()),
            Some(Arc::new(RwLock::new(
                PruneList::open(Some(db.clone()), "testpl").unwrap(),
            ))),
            Some(Arc::new(RwLock::new(
                LeafSet::open(Some(db.clone()), "testls").unwrap(),
            ))),
        )
        .unwrap();
        let mut backend2 = DiskBackend::new(
            db.clone(),
            Default::default(),
            Some(Default::default()),
            Some(Default::default()),
            Some(Arc::new(RwLock::new(
                PruneList::open(Some(db.clone()), "testpl").unwrap(),
            ))),
            Some(Arc::new(RwLock::new(
                LeafSet::open(Some(db.clone()), "testls").unwrap(),
            ))),
        )
        .unwrap();
        let mut backend3 = DiskBackend::new(
            db.clone(),
            Default::default(),
            Some(Default::default()),
            Some(Default::default()),
            Some(Arc::new(RwLock::new(
                PruneList::open(Some(db.clone()), "testpl").unwrap(),
            ))),
            Some(Arc::new(RwLock::new(
                LeafSet::open(Some(db), "testls").unwrap(),
            ))),
        )
        .unwrap();
        backend3.sector_config.as_mut().unwrap().sector_id = 1;
        let h1 = Hash256::hash_from_slice("test1", "asdf");
        let h2 = Hash256::hash_from_slice("test2", "asdf");
        backend1.write_leaf(h1, &vec![0x00, 0x01]);
        backend1.write_leaf(h2, &vec![0x01, 0x00]);

        assert_eq!(backend1.get_leaf(&h1), Ok(Some(vec![0x00, 0x01])));
        assert_eq!(backend1.get_leaf(&h2), Ok(Some(vec![0x01, 0x00])));
        assert_eq!(backend2.get_leaf(&h1), Ok(Some(vec![0x00, 0x01])));
        assert_eq!(backend2.get_leaf(&h2), Ok(Some(vec![0x01, 0x00])));
        assert_eq!(backend3.get_leaf(&h1), Ok(None));
        assert_eq!(backend3.get_leaf(&h2), Ok(None));
    }

    #[test]
    #[cfg(not(feature = "disable_tests_on_windows"))]
    fn prefix_iterator_forward() {
        let db = crate::chain::create_rocksdb_backend();
        let backend = DiskBackend::new(db, Default::default(), None, None, None, None).unwrap();

        backend.put("random_data1".as_bytes(), "asdf".to_owned());
        backend.put("test.1".as_bytes(), "asdf".to_owned());
        backend.put("random_data2".as_bytes(), "asdf".to_owned());
        backend.put("random_data3".as_bytes(), "asdf".to_owned());
        backend.put("test.2".as_bytes(), "asdf".to_owned());
        backend.put("test.3".as_bytes(), "asdf".to_owned());
        backend.put("random_data4".as_bytes(), "asdf".to_owned());

        let mut result = vec![];
        let mut iter: Box<dyn StreamingIterator<Item = (Vec<u8>, String)>> =
            backend.prefix_iterator("test.".as_bytes().to_vec(), IteratorDirection::Forward);

        while let Some((k, v)) = iter.next() {
            result.push((k.clone(), v.clone()));
        }

        assert_eq!(
            result,
            vec![
                ("test.1".as_bytes().to_vec(), "asdf".to_owned()),
                ("test.2".as_bytes().to_vec(), "asdf".to_owned()),
                ("test.3".as_bytes().to_vec(), "asdf".to_owned()),
            ]
        );
    }

    #[test]
    #[cfg(not(feature = "disable_tests_on_windows"))]
    fn prefix_iterator_backward() {
        let db = crate::chain::create_rocksdb_backend();
        let backend = DiskBackend::new(db, Default::default(), None, None, None, None).unwrap();

        backend.put("random_data1".as_bytes(), "asdf".to_owned());
        backend.put("test.1".as_bytes(), "asdf".to_owned());
        backend.put("random_data2".as_bytes(), "asdf".to_owned());
        backend.put("random_data3".as_bytes(), "asdf".to_owned());
        backend.put("test.2".as_bytes(), "asdf".to_owned());
        backend.put("test.3".as_bytes(), "asdf".to_owned());
        backend.put("random_data4".as_bytes(), "asdf".to_owned());

        let mut result = vec![];
        let mut iter: Box<dyn StreamingIterator<Item = (Vec<u8>, String)>> =
            backend.prefix_iterator("test.".as_bytes().to_vec(), IteratorDirection::Backward);

        while let Some((k, v)) = iter.next() {
            result.push((k.clone(), v.clone()));
        }

        assert_eq!(
            result,
            vec![
                ("test.3".as_bytes().to_vec(), "asdf".to_owned()),
                ("test.2".as_bytes().to_vec(), "asdf".to_owned()),
                ("test.1".as_bytes().to_vec(), "asdf".to_owned()),
            ]
        );
    }
}
