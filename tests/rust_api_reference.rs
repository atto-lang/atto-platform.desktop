use std::ffi::{CStr,c_char};
#[repr(C)] struct Config { abi_version:u32,width:u32,height:u32,max_events_per_frame:u32,frame_limit:u64,title:*const c_char,title_length:usize }
unsafe extern "C" {fn ores_desktop_default_config()->Config;fn ores_desktop_validate_config(config:*const Config)->i32;}
#[test] fn default_config_is_valid_and_owned(){let mut cfg=unsafe{ores_desktop_default_config()};assert_eq!(cfg.abi_version,2);assert_eq!((cfg.width,cfg.height),(800,600));assert!(cfg.max_events_per_frame>0 && cfg.max_events_per_frame<=256);assert_eq!(cfg.frame_limit,0);assert_eq!(cfg.title_length,8);assert_eq!(unsafe{CStr::from_ptr(cfg.title)}.to_bytes(),b"Oreslang");assert_eq!(unsafe{ores_desktop_validate_config(&cfg)},0);cfg.width=0;assert_eq!(unsafe{ores_desktop_validate_config(&cfg)},2);}

#[test]
fn default_config_adversarial_validation_matrix() {
    for case in 0..10 {
        let mut cfg = unsafe { ores_desktop_default_config() };
        match case {
            0 => cfg.abi_version = 0,
            1 => cfg.width = 0,
            2 => cfg.width = 16385,
            3 => cfg.height = 0,
            4 => cfg.height = 16385,
            5 => cfg.max_events_per_frame = 0,
            6 => cfg.max_events_per_frame = 257,
            7 => cfg.title_length = 0,
            8 => cfg.title_length = 128,
            9 => cfg.title = std::ptr::null(),
            _ => unreachable!()
        }
        assert_eq!(unsafe { ores_desktop_validate_config(&cfg) }, 2, "case={case}");
    }
    let mut cfg = unsafe { ores_desktop_default_config() };
    cfg.width = 16384;
    cfg.height = 16384;
    cfg.max_events_per_frame = 256;
    assert_eq!(unsafe { ores_desktop_validate_config(&cfg) }, 0);
    let invalid_utf8 = [0xf0u8, 0x80, 0x80, 0x80];
    cfg.title = invalid_utf8.as_ptr().cast();
    cfg.title_length = invalid_utf8.len();
    assert_eq!(unsafe { ores_desktop_validate_config(&cfg) }, 2);
    let valid_utf8 = "Ores 🦀".as_bytes();
    cfg.title = valid_utf8.as_ptr().cast();
    cfg.title_length = valid_utf8.len();
    assert_eq!(unsafe { ores_desktop_validate_config(&cfg) }, 0);
}
