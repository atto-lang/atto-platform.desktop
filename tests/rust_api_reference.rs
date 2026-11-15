use std::ffi::{CStr,c_char};
#[repr(C)] struct Config { abi_version:u32,width:u32,height:u32,max_events_per_frame:u32,frame_limit:u64,title:*const c_char,title_length:usize }
unsafe extern "C" {fn ores_desktop_default_config()->Config;fn ores_desktop_validate_config(config:*const Config)->i32;}
#[test] fn default_config_is_valid_and_owned(){let mut cfg=unsafe{ores_desktop_default_config()};assert_eq!(cfg.abi_version,2);assert_eq!((cfg.width,cfg.height),(800,600));assert!(cfg.max_events_per_frame>0 && cfg.max_events_per_frame<=256);assert_eq!(cfg.frame_limit,0);assert_eq!(cfg.title_length,8);assert_eq!(unsafe{CStr::from_ptr(cfg.title)}.to_bytes(),b"Oreslang");assert_eq!(unsafe{ores_desktop_validate_config(&cfg)},0);cfg.width=0;assert_eq!(unsafe{ores_desktop_validate_config(&cfg)},2);}
