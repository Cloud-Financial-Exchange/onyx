
# + shows the files used for ablation study
labels = [
    # # For motivation
    # "1k_100r_socket_spray_10000si_60s_jan12_630",

    # # For latency comparison
    # "5k_awstg_100r_50000si_150s_jan13_516",
    # "5k_iouring_spray_100r_50000si_150s_jan13_344",
    # "5k_jasper_100r_50000si_150s_jan13_358", # +
    # "dpdk_zc_spray_100r_jan30_735", # +
    # "dpdk_zc_spray_100r_jan30_742",

    # # For Hedging Intensity comparison
    # "5k_jasper_h0_50000si_150s_jan13_613", # +
    # "5k_jasper_h1_50000si_150s_jan13_631",
    # "5k_jasper_h2_50000si_150s_jan13_643",
    # "5k_jasper_h3_50000si_150s_jan13_657",
    # "5k_jasper_h4_50000si_150s_jan13_715",

    # # # For HoldNRelease
    # # "5k_jasper_hr_100r_50000si_150s_jan14_239",
    # "5k_jasper_hr_h2_100r_50000si_150s_jan14_302",
    # # "5k_jasper_hr_h2_100r_50000si_150s_jan14_330", # * +
    # "5k_jasper_hr_h0_100r_50000si_150s_jan14_317", # +
    # "5k_1p_100r_hr_50000si_150s_jan24_230",
    # # "5k_jasper_hr_h1_20us_1000r_50000si_150s_jan27_229",
    # # "5k_jasper_h1_50us_13b_2000r_50000si_150s_jan27_358",

    # # For Delta
    # "1k_jasper_h2_0us_19b_5kreceivers_10d_jan27_709_10000si_150s",

    # # For Scalability
    # "5k_jasper_100r_50000si_150s_jan13_358",
    # "1k_jasper_h1_100r_d10_10000si_150s_jan14_922",
    # "1k_jasper_h1_5832r_d18_10000si_150s_jan21_938",

    # Test
    # "5k_jasper_h1_100r_d10_10000si_150s_jan14_743",
    # "5k_tree_w_wo_hedging_parity_jan31_340",

    # "5k_iouring_spray_100r_50000si_150s_jan13_344",
    # "5k_jasper_h0_50000si_150s_jan13_613",
    # "5k_jasper_100r_50000si_150s_jan13_358", # +
    # "5k_jasper_hr_h0_100r_50000si_150s_jan14_317", # +
    # "5k_jasper_hr_h2_20us_100r_50000si_150s_jan14_330",

    # # "scaling"
    # "5k_jasper_100r_hr_50ms_501",
    # "5k_jasper_100r_hr_15ms_518", # 92%
    # "5k_jasper_1000r_hr_15ms_541", # 77%
    # "5k_jasper_5000r_hr_15ms_815",
    # "5k_jasper_100r__h1_hr_15ms_1133",
    # "5k_jasper_100r__h1_hr_15ms_1150",
    # "30k_h2_thpt_130",

    # Test
    # "test_1721242941453179",
    # "no_hedging_17july_314",
    # "h0_17july_326",
    # "h2_17july_341",
    # "h2_17july_347",
    # "h1_17july_400",
    # "h1_17july_403",

    # "h0_burst_17july_440", # total messages 470012
    # "h0_burst_17july_442",
    # "h0_burst_18july_1030", # 999958, 30 seconds, 10k-60k
    # "h0_burst_7x_18july_1036", 
    # "h0_burst_7x_18july_1039",
    # "h1_burst_7x_18july_1121",
    # "h1_burst_7x_18july_1123",
    
    # "h0_hist_burst_7x_19july_300",
    # "h0_hist_burst_7x_19july_302",
    # "h0_nohist_burst_7x_19july_308",
    # "h0_hist_test",
    # "h0_nohist_test",

    # "h0_hist_dyn_22july_1023",
    # "h0_hist_dyn_22july_1025",

    # "h0_nohist_dyn_22july_1039",

    # "h0_nohist_125r_22july_1106",
    # "h0_hist_125r_22july_1155",
    # "h0_hist_125r_22july_1157",
    # "h1_nohist_22july_1250",
    # "h0_hist_2size_22july_110",
    
    # "h1_nb_nohist_22july_201",
    # "h0_nb_hist_2size_22july_120",


    ######  Initial ######
    # "h0_nohist_125r_22july_1108",
    # "h0_nohist_dyn_23july_1153",

    # "h0_hist_fr_22july_255",
    # "h0_hist_2size_22july_113",

    # # "h1_nohist_22july_155",
    # "h1_nohist_23july_1036",
    # "h1_nohist_dyn_22july_625",

    # # "h1_nohist_dyn_22july_623",
    ###### Initial End ######

    # "h0_fr_23july_330",
    # "h0_fr_50kr_23july_424",
    # "h0_fr_50kr_3b_23july_428",

    # "h0_dr_hr_23july_351",  # not hr, just 5k actually
    # "h0_dr_50kr_23july_359",
    # "h0_dr_50kr_3b_23july_402",


    ####### Reprodubile Dynamicity Experiments #######
    # "h0_dyn_10k_15x_23july_612",
    # "h0_dyn_10k_15x_23july_650",
    # "h0_dyn_100k_1x_10s_23july_659",

    # "h0_fix_10k_15x_23july_629",
    # "h0_fix_10k_15x_23july_738",
    # "h0_fix_100k_1x_23july_742",
    ####### End Reprodubile Dynamicity Experiments #######


    ####### Small rate Dynamicity Experiments #######
    # "h0_fix_10k_24july_956",
    # "h0_fix_30k_24july_1114",
    # "h0_fix_10k_5x_24july_1120", # used in table

    # "h0_dyn_10k_24julu_1016",
    # "h0_dyn_30k_24julu_1022", # used in table
    # "h0_dyn_10k_5x_24julu_1026",
    ####### Small rate Dynamicity Experiments #######


    # "h0_fix_150k_24july_1132",
    # "h0_fix_200k_24july_1139",
    # "h0_fix_250k_24july_1146",

    # "h0_dyn_150k_24july_1239",
    # "h0_dyn_200k_24july_1246",
    # "h0_dyn_250k_24july_1250",


    # "h0_dyn_150k_25july_1226",
    # "h0_dyn_50k_25july_1232",
    # "h0_dyn_10k_15x_25july_121", # AWS

    # "h0_fix_150k_25july_211",
    # "h0_fix_150k_25july_103", # seems wrong
    # "h0_fix_50k_25july_106",
    # "h0_fix_10k_15x_25july_146",
    # "h0_fix_10k_15x_25july_219", # AWS

    # "h0_fix_10k_25r_25july_354",
    # "h0_dyn_10k_25r_25july_400",
    # "h0_dyn_10k_25r_25july_408",
    # "h0_fix_10k_25r_25july_415",
    
    # "h0_fix_10k_15x_25july_421",
    # "h0_fix_10k_15x_25july_423",
    # "h0_dyn_10k_15x_25july_431",
    # "h0_dyn_10k_15x_25july_432",

    # "h0_fix_dyn_10k_15x_25july_523",
    # "h0_fix_dyn_10k_15x_25july_531",

    # "h0_dyn_fix_25july_610", # 100 recvrs
    # "h0_dyn_10k_15x_100r_25july_647",
    # "h0_fix_10k_100r_25july_737",

    # "h0_fix_dyn_20k_15x_26july_941",
    # "h0_fix_dyn_20k_15x_26july_943",

    # "h0_dyn_20k_15x_26july_1004",
    # "h0_fix_20k_15x_26july_1026",
    # "h0_dyn_20k_15x_26july_11150",

    # "h0_dyn_100r_26july_106",
    # "h0_fix_100r_26july_122",

    # "100r_h1_25aug_1055",
    # "h1_200r_25aug_1118",
    # "h1_200r_25aug_1118_ALT",

    # Scaling message rate (GCP)
    # "h0_100k_100r_28aug_421",
    # "h0_150k_100r_28aug_422",
    # "h0_250k_100r_28aug_432",
    # "h0_350k_100r_28aug_440",

    # For loss rate
    # "100r_10k_10s_gcp_6sep_207", # (*20s)
    # "100r_100k_10s_gcp_6sep_210", # (*20s)
    # "100r_150k_10s_gcp_6sep_210", # (*20s)

    # "100r_10k_20s_aws_6sep_259",
    # "100r_100k_20s_aws_6sep_303",

    # New hold and release: Used for simultaneous delivery exps in Onyx
    # "5k_hr_h2_100r_sep13_459",
    # "cloudex_sep13_539",
    # "5k_hr_h0_sep13_618",

    ### GCP Proxy Hedging (with hold and release)
    # "100r_10k_120s", # H=2
    # "100r_10k_120s_dec9_114", # H=2
    # "h0_100r_10k_120s_dec11_1233",
    # "h0_100r_10k_120s_dec11_1237",

    ### GCP Proxy Hedging
    # "h2_100r_dec11_224",
    # "h2_100r_dec11_229",
    # "h0_100r_dec11_251",
    # "h0_100r_dec11_255",

    # GCP Holdrelease
    # "1000r_5k_hr_h2_dec13_413", # perfect fairness at 85p
    # "1000r_50k_hr_h2_dec13_430", # perfect fairness at 89p

    # Test
    # "socket_may_6_test",
    # "2socket_may_6_test",
    # "dpdk_test_25r",
    # "2dpdk_test_25r",
    # "dpdk_big_test",
    # "drop_test_may10_521",
    # "300s_may12_608",
    # "300s_may12_924",
    # "300s_175k_may12_937",
    # "600s_175k_159",
    # "600s_175k_100r_222",  # used for loss_exp
	
	# AWS artifact eval
    "onyx_100_rcvr",
    "du_100_rcvr",
]


texts = {
    "1k_100r_socket_spray_10000si_60s_jan12_630": "Direct Unicasts",

    "5k_jasper_100r_50000si_150s_jan13_358": "100 Receivers",
    "5k_iouring_spray_100r_50000si_150s_jan13_344": "DU",
    "5k_awstg_100r_50000si_150s_jan13_516": "AWS TG",
    # "dpdk_zc_spray_100r_jan30_735": "Spray w ZC DPDK",
    "dpdk_zc_spray_100r_jan30_742": "DU",

    "5k_jasper_h0_50000si_150s_jan13_613": "Jasper, H = 0",
    # "5k_jasper_h0_50000si_150s_jan13_613": "Jasper w/o Hedging",
    "5k_jasper_h1_50000si_150s_jan13_631": "Jasper, H = 1",
    # "5k_jasper_h1_50000si_150s_jan13_631": "Jasper, 100 Receivers",
    "5k_jasper_h2_50000si_150s_jan13_643": "Jasper, H = 2",
    "5k_jasper_h3_50000si_150s_jan13_657": "Jasper, H = 3",
    "5k_jasper_h4_50000si_150s_jan13_715": "Jasper, H = 4",

    "5k_jasper_hr_h0_100r_50000si_150s_jan14_317": "Jasper w/o hedging",
    "5k_jasper_hr_100r_50000si_150s_jan14_239": "Jasper, H=1",
    "5k_jasper_hr_h2_100r_50000si_150s_jan14_302": "Jasper, H=2",
    "5k_jasper_hr_h2_20us_100r_50000si_150s_jan14_330": "Tree + Hedging \n+ Fairness",
    "5k_1p_100r_hr_50000si_150s_jan24_230": "CloudEx",
    "5k_jasper_hr_h1_20us_1000r_50000si_150s_jan27_229": "Jasper, 1k Receivers, H=1",
    "5k_jasper_h1_50us_13b_2000r_50000si_150s_jan27_358": "Jasper, 2k Receivers, H=1, \u0394= 50us",

    "1k_jasper_h2_0us_19b_5kreceivers_10d_jan27_709_10000si_150s": "Jasper, 5k Receivers, H=2",

    "1k_jasper_h1_100r_d10_10000si_150s_jan14_922": "1K Receivers",
    "1k_jasper_h1_5832r_d18_10000si_150s_jan21_938": "5.8K Receivers",
    "5k_jasper_h1_5832r_d18_160000si_150s_jan21_944": "Jasper, 5.8K Receivers",

    # Test
    "5k_jasper_h1_100r_d10_10000si_150s_jan14_743": "Jasper 100r with 10 dup",
    "5k_tree_w_wo_hedging_parity_jan31_340": "Tree",
    "test_1721242430247821": "test",
    "test_1721242941453179": "test",
    "no_hedging_17july_314": "no_hedging_17july_314",
    "h0_17july_326": "H=0",
    "h2_17july_341": "H=2",
    "h2_17july_347": "H=2 .",
    "h1_17july_400": "H=1",
    "h1_17july_403": "H=1 .",
    "h0_burst_17july_440": "H=0, +Bursty",
    "h0_burst_17july_442": "H=0, +Bursty .",
    "h0_burst_18july_1030": "H=0, +Bursty",
    "h0_burst_7x_18july_1036": "h0_burst_7x_18july_1036",
    "h0_burst_7x_18july_1039": "h0_burst_7x_18july_1039",
    "h1_burst_7x_18july_1121": "h1_burst_7x_18july_1121",
    "h1_burst_7x_18july_1123": "h1_burst_7x_18july_1123",
    "h2_burst_7x_18july_1143": "h2_burst_7x_18july_1143",
    "h2_burst_7x_18july_1146": "h2_burst_7x_18july_1146",
    "h2_burst_5x_18july_219": "h2_burst_5x_18july_219",
    "h2_burst_5x_18july_225": "h2_burst_5x_18july_225",
    "h0_hist_burst_7x_19july_300": "h0_hist_burst_7x_19july_300",
    "h0_hist_burst_7x_19july_302": "h0_hist_burst_7x_19july_302",
    "h0_nohist_burst_7x_19july_308": "h0_nohist_burst_7x_19july_308",
    "h0_hist_test": "h0_hist_test",
    "h0_nohist_test": "h0_nohist_test",
    "h0_hist_dyn_22july_1023": "h0_hist_dyn_22july_1023",
    "h0_hist_dyn_22july_1025": "h0_hist_dyn_22july_1025",
    "h0_nohist_dyn_22july_1039": "h0_nohist_dyn_22july_1039",
    "h0_nohist_125r_22july_1106": "H=0, No History",
    "h0_nohist_125r_22july_1108": "H=0, No History, Fixed Relationships",
    "h0_hist_125r_22july_1155": "H=0, History",
    "h0_hist_125r_22july_1157": "H=0, History",
    "h1_nohist_22july_1250": "H=1, No History",
    "h0_hist_2size_22july_110": "h0_hist_2size_22july_110",
    "h0_hist_2size_22july_113": "H=0, History, Dynamic Relationships",
    "h0_nb_hist_2size_22july_120": "h0_nb_hist_2size_22july_120",
    "h1_nohist_22july_155": "H=1, No hist, Fixed Relationships",
    "h0_hist_fr_22july_255": "H=0, History, Fixed Relationships",
    "h1_nohist_dyn_22july_623": "H=1, No hist, Dynamic Relationships",
    "h1_nohist_dyn_22july_625": "H=1, No hist, Dynamic Relationships",
    "h1_nohist_fr_22july_705": "H=1, No hist, Fixed Relationships",
    "h1_nohist_fr_22july_707": "H=1, No hist, Fixed Relationships",
    "h1_nohist_23july_1036": "H=1, No hist, Fixed Relationships",
    "h0_nohist_dyn_23july_1153": "H=0, No hist, Dynamic Relationships",


    "h0_hist_fr_hr_23july_116": "H=0, Hist, Fixed, High rate",
    "h0_nohist_fr_hr_23july_142": "H=0, No hist, Fixed, High rate",

    "h0_fr_23july_330": "H=0, Fixed, 5k crate",
    "h0_dr_hr_23july_351": "H=0, Dynamic, 5k crate",

    "h0_dr_50kr_23july_359": "H=0, Dynamic, 50k crate",
    "h0_dr_50kr_3b_23july_402": "H=0, Dynamic, 50k with 3x burst",
    "h0_fr_50kr_23july_424": "H=0, Fixed, 50k crate",
    "h0_fr_50kr_3b_23july_428": "H=0, Fixed, 50K with 3x burst",

    "h0_dyn_10k_15x_23july_612": "Dynamic Relationships",
    "h0_fix_10k_15x_23july_629": "Fix Relationships",
    "h0_dyn_10k_15x_23july_650": "h0_dyn_10k_15x_23july_650",
    "h0_dyn_100k_1x_10s_23july_659": "Dyn 100k",
    "h0_fix_10k_15x_23july_738": "h0_fix_10k_15x_23july_738",
    "h0_fix_100k_1x_23july_742": "Fix, 100k",
    "h0_fix_10k_24july_956": "h0_fix_10k_24july_956",
    "h0_dyn_10k_24julu_1016": "h0_dyn_10k_24julu_1016",
    "h0_dyn_30k_24julu_1022": "h0_dyn_30k_24julu_1022",
    "h0_dyn_10k_5x_24julu_1026": "h0_dyn_10k_5x_24julu_1026",
    "h0_fix_10k_24july_1108": "h0_fix_10k_24july_1108",
    "h0_fix_30k_24july_1114": "h0_fix_30k_24july_1114",
    "h0_fix_10k_5x_24july_1120": "h0_fix_10k_5x_24july_1120",
    "h0_fix_150k_24july_1132": "Fix, 150k",
    "h0_fix_200k_24july_1139": "Fix, 200k",
    "h0_fix_250k_24july_1146": "Fix, 250k",
    "h0_dyn_150k_24july_1239": "Dyn, 150k",
    "h0_dyn_200k_24july_1246": "Dyn, 200k",
    "h0_dyn_250k_24july_1250": "Dyn, 250k",
    "h0_dyn_150k_25july_1226": "h0_dyn_150k_25july_1226",
    "h0_dyn_50k_25july_1232": "h0_dyn_50k_25july_1232",
    "h0_fix_150k_25july_103": "h0_fix_150k_25july_103",
    "h0_fix_50k_25july_106": "h0_fix_50k_25july_106",
    "h0_dyn_10k_15x_25july_121": "Dynamic Relationships, AWS",
    "h0_fix_10k_15x_25july_146": "Fix Relationships",
    "h0_fix_150k_25july_211": "h0_fix_150k_25july_211",
    "h0_fix_10k_15x_25july_219": "Fix Relationships, AWS",
    "h0_fix_10k_25r_25july_354": "Fixed Relationships, 2 Hops",
    "h0_dyn_10k_25r_25july_400": "Dynamic Relationships, 2 Hops",
    "h0_dyn_10k_25r_25july_408": "Dynamic Relationships, 2 Hops",
    "h0_fix_10k_25r_25july_415": "Fixed Relationships, 2 Hops",
    "h0_fix_10k_15x_25july_421": "h0_fix_10k_15x_25july_421",
    "h0_fix_10k_15x_25july_423": "h0_fix_10k_15x_25july_423",
    "h0_dyn_10k_15x_25july_431": "h0_dyn_10k_15x_25july_431",
    "h0_dyn_10k_15x_25july_432": "h0_dyn_10k_15x_25july_432",
    "h0_fix_dyn_10k_15x_25july_523": "h0_fix_dyn_10k_15x_25july_523",
    "h0_fix_dyn_10k_15x_25july_531": "h0_fix_dyn_10k_15x_25july_531",
    "h0_dyn_fix_25july_610": "h0_dyn_fix_25july_610",
    "h0_dyn_10k_15x_100r_25july_647": "h0_dyn_10k_15x_100r_25july_647",
    "h0_fix_10k_100r_25july_737": "h0_fix_10k_100r_25july_737",
    "h0_fix_dyn_20k_15x_26july_941": "h0_fix_dyn_20k_15x_26july_941",
    "h0_fix_dyn_20k_15x_26july_943": "h0_fix_dyn_20k_15x_26july_943",
    "h0_dyn_20k_15x_26july_1004": "h0_dyn_20k_15x_26july_1004",
    "h0_fix_20k_15x_26july_1026": "h0_fix_20k_15x_26july_1026",
    "h0_dyn_20k_15x_26july_11150": "h0_dyn_20k_15x_26july_11150",
    "h0_dyn_100r_26july_106": "Dyn, 150k rate",
    "h0_fix_100r_26july_122": "Fix, 150k rate",


    "5k_jasper_100r_hr_50ms_501": "100r",
    "5k_jasper_100r_hr_15ms_518": "100r",
    "5k_jasper_1000r_hr_15ms_541": "1k r",
    "5k_jasper_5000r_hr_15ms_815": "5k",
    "5k_jasper_100r__h1_hr_15ms_1133": "100r",
    "5k_jasper_100r__h1_hr_15ms_1150": "100r",
    "30k_h2_thpt_130": "100r, 35 MPS, H=2",

    "100r_h1_25aug_1055": "100r_h1_25aug_1055",
    "h1_200r_25aug_1118": "h1_200r_25aug_1118",
    "h1_200r_25aug_1118_ALT": "h1_200r_25aug_1118_ALT",

    "h0_100k_100r_28aug_421": "h0_100k_100r_28aug_421",
    "h0_150k_100r_28aug_422": "h0_150k_100r_28aug_422",
    "h0_250k_100r_28aug_432": "h0_250k_100r_28aug_432",
    "h0_350k_100r_28aug_440": "h0_350k_100r_28aug_440",

    "5k_hr_h2_100r_sep13_454": "Onyx, H=2",
    "5k_hr_h2_100r_sep13_459": "Onyx, H=2",
    "cloudex_sep13_539": "CloudEx",
    "5k_hr_h0_sep13_628": "Onyx, H=0",
    "5k_hr_h0_sep13_618": "Onyx, H=0",

    "100r_10k_120s": "100r_10k_120s",
    "100r_10k_120s_dec9_114": "100r_10k_120s_dec9_114",
    "h0_100r_10k_120s_dec11_1233": "h0_100r_10k_120s_dec11_1233",
    "h0_100r_10k_120s_dec11_1237": "h0_100r_10k_120s_dec11_1237",
    "h2_100r_dec11_224": "h = 2, 100 R",
    "h2_100r_dec11_229": "h = 2, 100 R",
    "h0_100r_dec11_251": "h = 0, 100 R",
    "h0_100r_dec11_255": "h = 0, 100 R",
    "1000r_5k_hr_h2_dec13_413": "1000r_5k_hr_h2_dec13_413",
    "1000r_5k_hr_h2_dec13_424": "1000r_5k_hr_h2_dec13_424",
    "1000r_50k_hr_h2_dec13_430": "1000r_50k_hr_h2_dec13_430",

    "socket_may_6_test": "socket_may_6_test",
    "2socket_may_6_test": "socket_may_6_test",
	
    # AE..
    "onyx_100_rcvr": "Onyx",
    "du_100_rcvr": "DU",
}

info = {
    "10k_awstg_100000si_600s_1151": {"num_receivers": 100, "msg_rate": 10000, "total_seconds": 600, "starting_id": 100000, "parity": False},
    "5k_sh_50000si_600s_jan1_1235": {"num_receivers": 100, "msg_rate": 5000, "total_seconds": 600, "starting_id": 50000, "parity": False},
    "5k_spray_60000si_600s": {"num_receivers": 100, "msg_rate": 5000, "total_seconds": 600, "starting_id": 60000, "parity": False},
    "5k_awstg_50000si_600s": {"num_receivers": 100, "msg_rate": 5000, "total_seconds": 600, "starting_id": 50000, "parity": False},

    "1k_100r_socket_spray_10000si_60s_jan12_630": {"num_receivers": 100, "msg_rate": 1000, "total_seconds": 60, "starting_id": 10000, "parity": False},
    "5k_iouring_spray_100r_50000si_150s_jan13_344": {"num_receivers": 100, "msg_rate": 5000, "total_seconds": 150, "starting_id": 50000, "parity": False},
    "5k_jasper_100r_50000si_150s_jan13_358": {"num_receivers": 100, "msg_rate": 5000, "total_seconds": 150, "starting_id": 50000, "parity": False},
    "5k_awstg_100r_50000si_150s_jan13_516": {"num_receivers": 100, "msg_rate": 5000, "total_seconds": 150, "starting_id": 50000, "parity": False},
    "5k_jasper_h0_50000si_150s_jan13_613": {"num_receivers": 100, "msg_rate": 5000, "total_seconds": 150, "starting_id": 50000, "parity": False},
    "5k_jasper_h1_50000si_150s_jan13_631": {"num_receivers": 100, "msg_rate": 5000, "total_seconds": 150, "starting_id": 50000, "parity": False},
    "5k_jasper_h2_50000si_150s_jan13_643": {"num_receivers": 100, "msg_rate": 5000, "total_seconds": 150, "starting_id": 50000, "parity": False},

    "5k_jasper_h3_50000si_150s_jan13_657": {"num_receivers": 100, "msg_rate": 5000, "total_seconds": 150, "starting_id": 50000, "parity": False},
    "5k_jasper_h4_50000si_150s_jan13_715": {"num_receivers": 100, "msg_rate": 5000, "total_seconds": 150, "starting_id": 50000, "parity": False},

    "5k_jasper_hr_h0_100r_50000si_150s_jan14_317": {"num_receivers": 100, "msg_rate": 5000, "total_seconds": 150, "starting_id": 50000, "parity": False},
    "5k_jasper_hr_100r_50000si_150s_jan14_239": {"num_receivers": 100, "msg_rate": 5000, "total_seconds": 150, "starting_id": 50000, "parity": False},
    "5k_jasper_hr_h2_100r_50000si_150s_jan14_302": {"num_receivers": 100, "msg_rate": 5000, "total_seconds": 150, "starting_id": 50000, "parity": False},
    "5k_jasper_hr_h2_20us_100r_50000si_150s_jan14_330": {"num_receivers": 100, "msg_rate": 5000, "total_seconds": 150, "starting_id": 50000, "parity": False},
    "5k_jasper_hr_h1_20us_1000r_50000si_150s_jan27_229": {"num_receivers": 1000, "msg_rate": 5000, "total_seconds": 150, "starting_id": 50000, "parity": False},
    "5k_jasper_h1_50us_13b_2000r_50000si_150s_jan27_358": {"num_receivers": 2000, "msg_rate": 5000, "total_seconds": 150, "starting_id": 50000, "parity": False},
    "1k_jasper_h2_0us_19b_5kreceivers_10d_jan27_709_10000si_150s": {"num_receivers": 5000, "msg_rate": 1000, "total_seconds": 150, "starting_id": 10000, "parity": False},

    "1k_jasper_h1_100r_d10_10000si_150s_jan14_854": {"num_receivers": 1000, "msg_rate": 1000, "total_seconds": 150, "starting_id": 10000, "parity": False},
    "1k_jasper_h1_100r_d10_10000si_150s_jan14_922": {"num_receivers": 1000, "msg_rate": 1000, "total_seconds": 150, "starting_id": 10000, "parity": False},
    "1k_jasper_h1_5832r_d18_10000si_150s_jan21_938": {"num_receivers": 5832, "msg_rate": 1000, "total_seconds": 150, "starting_id": 10000, "parity": False},
    "5k_jasper_h1_5832r_d18_160000si_150s_jan21_944": {"num_receivers": 5832, "msg_rate": 5000, "total_seconds": 150, "starting_id": 10000, "parity": False},
    "5k_1p_100r_hr_50000si_150s_jan24_230": {"num_receivers": 100, "msg_rate": 5000, "total_seconds": 150, "starting_id": 50000, "parity": False},


    # Test
    "5k_jasper_h1_100r_d10_10000si_150s_jan14_743": {"num_receivers": 100, "msg_rate": 1000, "total_seconds": 150, "starting_id": 10000, "parity": False},
    "dpdk_zc_spray_100r_jan30_735": {"num_receivers": 100, "msg_rate": 5000, "total_seconds": 150, "starting_id": 10000, "parity": False},
    "dpdk_zc_spray_100r_jan30_742": {"num_receivers": 100, "msg_rate": 5000, "total_seconds": 150, "starting_id": 10000, "parity": False},
    "5k_tree_w_wo_hedging_parity_jan31_340": {"num_receivers": 100, "msg_rate": 5000, "total_seconds": 300, "starting_id": 50000, "parity": True},
    "test_1721242430247821": {"num_receivers": 100, "msg_rate": 10000, "total_seconds": 10, "starting_id": 200000, "parity": False},
    "test_1721242941453179": {"num_receivers": 100, "msg_rate": 10000, "total_seconds": 10, "starting_id": 200000, "parity": False},
    "no_hedging_17july_314": {"num_receivers": 100, "msg_rate": 10000, "total_seconds": 120, "starting_id": 200000, "parity": False},
    "h0_17july_326": {"num_receivers": 100, "msg_rate": 10000, "total_seconds": 20, "starting_id": 1600000, "parity": False},
    "h2_17july_341": {"num_receivers": 100, "msg_rate": 10000, "total_seconds": 20, "starting_id": 1600000, "parity": False},
    "h2_17july_347": {"num_receivers": 100, "msg_rate": 10000, "total_seconds": 20, "starting_id": 1600000, "parity": False},
    "h1_17july_400": {"num_receivers": 100, "msg_rate": 10000, "total_seconds": 20, "starting_id": 1600000, "parity": False},
    "h1_17july_403": {"num_receivers": 100, "msg_rate": 10000, "total_seconds": 20, "starting_id": 1600000, "parity": False},
    "h0_burst_17july_440": {"num_receivers": 100, "msg_rate": 10000, "total_seconds": 20, "starting_id": 1600000, "parity": False},
    "h0_burst_17july_442": {"num_receivers": 100, "msg_rate": 10000, "total_seconds": 20, "starting_id": 1600000, "parity": False},
    "h0_burst_18july_1030": {"num_receivers": 100, "msg_rate": 10000, "total_seconds": 30, "starting_id": 1600000, "parity": False},
    "h0_burst_7x_18july_1036": {"num_receivers": 100, "msg_rate": 10000, "total_seconds": 30, "starting_id": 1600000, "parity": False},
    "h0_burst_7x_18july_1039": {"num_receivers": 100, "msg_rate": 10000, "total_seconds": 30, "starting_id": 1600000, "parity": False},
    "h1_burst_7x_18july_1121": {"num_receivers": 100, "msg_rate": 10000, "total_seconds": 30, "starting_id": 1600000, "parity": False},
    "h1_burst_7x_18july_1123": {"num_receivers": 100, "msg_rate": 10000, "total_seconds": 30, "starting_id": 1600000, "parity": False},
    "h2_burst_7x_18july_1143": {"num_receivers": 100, "msg_rate": 10000, "total_seconds": 30, "starting_id": 1600000, "parity": False},
    "h2_burst_7x_18july_1146": {"num_receivers": 100, "msg_rate": 10000, "total_seconds": 30, "starting_id": 1600000, "parity": False},
    "h2_burst_5x_18july_219": {"num_receivers": 100, "msg_rate": 10000, "total_seconds": 30, "starting_id": 1600000, "parity": False},
    "h2_burst_5x_18july_225": {"num_receivers": 100, "msg_rate": 10000, "total_seconds": 30, "starting_id": 1600000, "parity": False},
    "h0_hist_burst_7x_19july_300": {"num_receivers": 100, "msg_rate": 10000, "total_seconds": 30, "starting_id": 1600000, "parity": False},
    "h0_hist_burst_7x_19july_302": {"num_receivers": 100, "msg_rate": 10000, "total_seconds": 30, "starting_id": 1600000, "parity": False},
    "h0_nohist_burst_7x_19july_308": {"num_receivers": 100, "msg_rate": 10000, "total_seconds": 30, "starting_id": 1600000, "parity": False},
    "h0_hist_test": {"num_receivers": 100, "msg_rate": 10000, "total_seconds": 30, "starting_id": 1600000, "parity": False},
    "h0_nohist_test": {"num_receivers": 100, "msg_rate": 10000, "total_seconds": 30, "starting_id": 1600000, "parity": False},
    "h0_hist_dyn_22july_1023": {"num_receivers": 100, "msg_rate": 10000, "total_seconds": 30, "starting_id": 1600000, "parity": False},
    "h0_hist_dyn_22july_1025": {"num_receivers": 100, "msg_rate": 10000, "total_seconds": 30, "starting_id": 1600000, "parity": False},
    "h0_nohist_dyn_22july_1039": {"num_receivers": 100, "msg_rate": 10000, "total_seconds": 30, "starting_id": 1600000, "parity": False},
    "h0_nohist_125r_22july_1106": {"num_receivers": 125, "msg_rate": 10000, "total_seconds": 30, "starting_id": 1600000, "parity": False},
    "h0_nohist_125r_22july_1108": {"num_receivers": 125, "msg_rate": 10000, "total_seconds": 30, "starting_id": 1600000, "parity": False},
    "h0_hist_125r_22july_1155": {"num_receivers": 125, "msg_rate": 10000, "total_seconds": 30, "starting_id": 1600000, "parity": False},
    "h0_hist_125r_22july_1157": {"num_receivers": 125, "msg_rate": 10000, "total_seconds": 30, "starting_id": 1600000, "parity": False},
    "h1_nohist_22july_1250": {"num_receivers": 125, "msg_rate": 10000, "total_seconds": 30, "starting_id": 1600000, "parity": False},
    "h0_hist_2size_22july_110": {"num_receivers": 125, "msg_rate": 10000, "total_seconds": 30, "starting_id": 1600000, "parity": False},
    "h0_hist_2size_22july_113": {"num_receivers": 125, "msg_rate": 10000, "total_seconds": 30, "starting_id": 1600000, "parity": False},
    "h0_nb_hist_2size_22july_120": {"num_receivers": 125, "msg_rate": 10000, "total_seconds": 30, "starting_id": 1600000, "parity": False},
    "h1_nohist_22july_155": {"num_receivers": 125, "msg_rate": 10000, "total_seconds": 30, "starting_id": 1600000, "parity": False},
    "h0_hist_fr_22july_255": {"num_receivers": 125, "msg_rate": 10000, "total_seconds": 30, "starting_id": 1600000, "parity": False},
    "h1_nohist_dyn_22july_623": {"num_receivers": 125, "msg_rate": 10000, "total_seconds": 30, "starting_id": 1600000, "parity": False},
    "h1_nohist_dyn_22july_625": {"num_receivers": 125, "msg_rate": 10000, "total_seconds": 30, "starting_id": 1600000, "parity": False},
    "h1_nohist_fr_22july_705": {"num_receivers": 125, "msg_rate": 10000, "total_seconds": 30, "starting_id": 1600000, "parity": False},
    "h1_nohist_fr_22july_707": {"num_receivers": 125, "msg_rate": 10000, "total_seconds": 30, "starting_id": 1600000, "parity": False},
    "h1_nohist_23july_1036": {"num_receivers": 125, "msg_rate": 10000, "total_seconds": 30, "starting_id": 1600000, "parity": False},
    "h0_nohist_dyn_23july_1153": {"num_receivers": 125, "msg_rate": 10000, "total_seconds": 30, "starting_id": 1600000, "parity": False},
    "h0_hist_fr_hr_23july_116": {"num_receivers": 125, "msg_rate": 150000, "total_seconds": 15, "starting_id": 1600000, "parity": False},
    "h0_nohist_fr_hr_23july_142": {"num_receivers": 125, "msg_rate": 150000, "total_seconds": 15, "starting_id": 1600000, "parity": False},
    "h0_fr_23july_330": {"num_receivers": 125, "msg_rate": 5000, "total_seconds": 60, "starting_id": 1600000, "parity": False},
    "h0_dr_hr_23july_351": {"num_receivers": 125, "msg_rate": 5000, "total_seconds": 60, "starting_id": 1600000, "parity": False},
    "h0_dr_50kr_23july_359": {"num_receivers": 125, "msg_rate": 50000, "total_seconds": 30, "starting_id": 1600000, "parity": False},
    "h0_dr_50kr_3b_23july_402": {"num_receivers": 125, "msg_rate": 50000, "total_seconds": 30, "starting_id": 1600000, "parity": False},
    "h0_fr_50kr_23july_424": {"num_receivers": 125, "msg_rate": 50000, "total_seconds": 30, "starting_id": 1600000, "parity": False},
    "h0_fr_50kr_3b_23july_428": {"num_receivers": 125, "msg_rate": 50000, "total_seconds": 30, "starting_id": 1600000, "parity": False},
    "h0_dyn_10k_15x_23july_612": {"num_receivers": 125, "msg_rate": 10000, "total_seconds": 30, "starting_id": 1600000, "parity": False},
    "h0_fix_10k_15x_23july_629": {"num_receivers": 125, "msg_rate": 10000, "total_seconds": 30, "starting_id": 1600000, "parity": False},
    "h0_dyn_10k_15x_23july_650": {"num_receivers": 125, "msg_rate": 10000, "total_seconds": 30, "starting_id": 1600000, "parity": False},
    "h0_dyn_100k_1x_10s_23july_659": {"num_receivers": 125, "msg_rate": 100000, "total_seconds": 10, "starting_id": 1600000, "parity": False},
    "h0_fix_10k_15x_23july_738": {"num_receivers": 125, "msg_rate": 10000, "total_seconds": 30, "starting_id": 1600000, "parity": False},
    "h0_fix_100k_1x_23july_742": {"num_receivers": 125, "msg_rate": 100000, "total_seconds": 10, "starting_id": 1600000, "parity": False},
    "h0_fix_10k_24july_956": {"num_receivers": 125, "msg_rate": 10000, "total_seconds": 30, "starting_id": 1600000, "parity": False},
    "h0_dyn_10k_24julu_1016": {"num_receivers": 125, "msg_rate": 10000, "total_seconds": 30, "starting_id": 1600000, "parity": False},
    "h0_dyn_30k_24julu_1022": {"num_receivers": 125, "msg_rate": 30000, "total_seconds": 30, "starting_id": 1600000, "parity": False},
    "h0_dyn_10k_5x_24julu_1026": {"num_receivers": 125, "msg_rate": 10000, "total_seconds": 30, "starting_id": 1600000, "parity": False},
    "h0_fix_10k_24july_1108": {"num_receivers": 125, "msg_rate": 10000, "total_seconds": 30, "starting_id": 1600000, "parity": False},
    "h0_fix_30k_24july_1114": {"num_receivers": 125, "msg_rate": 30000, "total_seconds": 30, "starting_id": 1600000, "parity": False},
    "h0_fix_10k_5x_24july_1120": {"num_receivers": 125, "msg_rate": 10000, "total_seconds": 30, "starting_id": 1600000, "parity": False},
    "h0_fix_150k_24july_1132": {"num_receivers": 125, "msg_rate": 150000, "total_seconds": 10, "starting_id": 1600000, "parity": False},
    "h0_fix_200k_24july_1139": {"num_receivers": 125, "msg_rate": 200000, "total_seconds": 10, "starting_id": 1600000, "parity": False},
    "h0_fix_250k_24july_1146": {"num_receivers": 125, "msg_rate": 250000, "total_seconds": 10, "starting_id": 1600000, "parity": False},
    "h0_dyn_150k_24july_1239": {"num_receivers": 125, "msg_rate": 150000, "total_seconds": 10, "starting_id": 1600000, "parity": False},
    "h0_dyn_200k_24july_1246": {"num_receivers": 125, "msg_rate": 200000, "total_seconds": 10, "starting_id": 1600000, "parity": False},
    "h0_dyn_250k_24july_1250": {"num_receivers": 125, "msg_rate": 250000, "total_seconds": 10, "starting_id": 1600000, "parity": False},
    "h0_dyn_150k_25july_1226": {"num_receivers": 125, "msg_rate": 150000, "total_seconds": 20, "starting_id": 1600000, "parity": False},
    "h0_dyn_50k_25july_1232": {"num_receivers": 125, "msg_rate": 50000, "total_seconds": 20, "starting_id": 1600000, "parity": False},
    "h0_fix_150k_25july_103": {"num_receivers": 125, "msg_rate": 150000, "total_seconds": 20, "starting_id": 1600000, "parity": False},
    "h0_fix_50k_25july_106": {"num_receivers": 125, "msg_rate": 50000, "total_seconds": 20, "starting_id": 1600000, "parity": False},
    "h0_dyn_10k_15x_25july_121": {"num_receivers": 125, "msg_rate": 10000, "total_seconds": 20, "starting_id": 1600000, "parity": False},
    "h0_fix_10k_15x_25july_146": {"num_receivers": 125, "msg_rate": 10000, "total_seconds": 20, "starting_id": 1600000, "parity": False},
    "h0_fix_150k_25july_211": {"num_receivers": 125, "msg_rate": 150000, "total_seconds": 20, "starting_id": 1600000, "parity": False},
    "h0_fix_10k_15x_25july_219": {"num_receivers": 125, "msg_rate": 10000, "total_seconds": 20, "starting_id": 1600000, "parity": False},
    "h0_fix_10k_25r_25july_354": {"num_receivers": 25, "msg_rate": 10000, "total_seconds": 30, "starting_id": 1600000, "parity": False},
    "h0_dyn_10k_25r_25july_400": {"num_receivers": 25, "msg_rate": 10000, "total_seconds": 30, "starting_id": 1600000, "parity": False},
    "h0_dyn_10k_25r_25july_408": {"num_receivers": 25, "msg_rate": 10000, "total_seconds": 30, "starting_id": 1600000, "parity": False},
    "h0_fix_10k_25r_25july_415": {"num_receivers": 25, "msg_rate": 10000, "total_seconds": 30, "starting_id": 1600000, "parity": False},
    "h0_fix_10k_15x_25july_421": {"num_receivers": 25, "msg_rate": 10000, "total_seconds": 30, "starting_id": 1600000, "parity": False},
    "h0_fix_10k_15x_25july_423": {"num_receivers": 25, "msg_rate": 10000, "total_seconds": 30, "starting_id": 1600000, "parity": False},
    "h0_dyn_10k_15x_25july_431": {"num_receivers": 25, "msg_rate": 10000, "total_seconds": 30, "starting_id": 1600000, "parity": False},
    "h0_dyn_10k_15x_25july_432": {"num_receivers": 25, "msg_rate": 10000, "total_seconds": 30, "starting_id": 1600000, "parity": False},
    "h0_fix_dyn_10k_15x_25july_523": {"num_receivers": 125, "msg_rate": 10000, "total_seconds": 40, "starting_id": 1600000, "parity": True},
    "h0_fix_dyn_10k_15x_25july_531": {"num_receivers": 125, "msg_rate": 10000, "total_seconds": 40, "starting_id": 1600000, "parity": True},
    "h0_dyn_fix_25july_610": {"num_receivers": 100, "msg_rate": 10000, "total_seconds": 40, "starting_id": 1600000, "parity": True},
    "h0_dyn_10k_15x_100r_25july_647": {"num_receivers": 100, "msg_rate": 10000, "total_seconds": 40, "starting_id": 1600000, "parity": False},
    "h0_fix_10k_100r_25july_737": {"num_receivers": 100, "msg_rate": 10000, "total_seconds": 40, "starting_id": 1600000, "parity": False},
    "h0_fix_dyn_20k_15x_26july_941": {"num_receivers": 125, "msg_rate": 20000, "total_seconds": 10, "starting_id": 1600000, "parity": True},
    "h0_fix_dyn_20k_15x_26july_943": {"num_receivers": 125, "msg_rate": 20000, "total_seconds": 10, "starting_id": 1600000, "parity": True},
    "h0_dyn_20k_15x_26july_1004": {"num_receivers": 125, "msg_rate": 20000, "total_seconds": 20, "starting_id": 1600000, "parity": False},
    "h0_fix_20k_15x_26july_1026": {"num_receivers": 125, "msg_rate": 20000, "total_seconds": 20, "starting_id": 1600000, "parity": False},
    "h0_dyn_20k_15x_26july_11150": {"num_receivers": 125, "msg_rate": 20000, "total_seconds": 20, "starting_id": 1600000, "parity": False},
    "h0_dyn_100r_26july_106": {"num_receivers": 100, "msg_rate": 150000, "total_seconds": 20, "starting_id": 1600000, "parity": False},
    "h0_fix_100r_26july_122": {"num_receivers": 100, "msg_rate": 150000, "total_seconds": 20, "starting_id": 1600000, "parity": False},
    "100r_h1_25aug_1055": {"num_receivers": 100, "msg_rate": 10000, "total_seconds": 60, "starting_id": 1600000, "parity": False},
    "h1_200r_25aug_1118_ALT": {"num_receivers": 100, "msg_rate": 10000, "total_seconds": 60, "starting_id": 1600000, "parity": False},
    "h0_100k_100r_28aug_421": {"num_receivers": 100, "msg_rate": 100000, "total_seconds": 5, "starting_id": 1100097, "parity": False},
    "h0_150k_100r_28aug_422": {"num_receivers": 100, "msg_rate": 150000, "total_seconds": 5, "starting_id": 2349854, "parity": False},
    "h0_250k_100r_28aug_432": {"num_receivers": 100, "msg_rate": 250000, "total_seconds": 5, "starting_id": 2349854, "parity": False},
    "h0_350k_100r_28aug_440": {"num_receivers": 100, "msg_rate": 350000, "total_seconds": 5, "starting_id": 2349854, "parity": False},


    "5k_jasper_100r_hr_50ms_501": {"num_receivers": 100, "msg_rate": 5000, "total_seconds": 150, "starting_id": 50000, "parity": False},
    "5k_jasper_100r_hr_15ms_518": {"num_receivers": 100, "msg_rate": 5000, "total_seconds": 150, "starting_id": 50000, "parity": False},
    "5k_jasper_1000r_hr_15ms_541": {"num_receivers": 1000, "msg_rate": 5000, "total_seconds": 150, "starting_id": 50000, "parity": False},
    "5k_jasper_5000r_hr_15ms_815": {"num_receivers": 5000, "msg_rate": 5000, "total_seconds": 150, "starting_id": 50000, "parity": False},
    "5k_jasper_100r__h1_hr_15ms_1133": {"num_receivers": 100, "msg_rate": 5000, "total_seconds": 150, "starting_id": 50000, "parity": False},
    "5k_jasper_100r__h1_hr_15ms_1150": {"num_receivers": 100, "msg_rate": 5000, "total_seconds": 150, "starting_id": 50000, "parity": False},
    "30k_h2_thpt_130": {"num_receivers": 100, "msg_rate": 35000, "total_seconds": 150, "starting_id": 50000, "parity": False},
    
    "socket_may_6_test": {"num_receivers": 100, "msg_rate": 5000, "total_seconds": 120, "starting_id": 50000, "parity": False},
    "2socket_may_6_test": {"num_receivers": 100, "msg_rate": 5000, "total_seconds": 120, "starting_id": 50000, "parity": False},
    
    
    "5k_hr_h2_100r_sep13_454": {"num_receivers": 100, "msg_rate": 5000, "total_seconds": 30, "starting_id": 200040, "parity": False},
    "5k_hr_h2_100r_sep13_459": {"num_receivers": 100, "msg_rate": 5000, "total_seconds": 150, "starting_id": 1350033, "parity": False},
    "cloudex_sep13_539": {"num_receivers": 100, "msg_rate": 5000, "total_seconds": 150, "starting_id": 1350033, "parity": False},
    "5k_hr_h0_sep13_618": {"num_receivers": 100, "msg_rate": 5000, "total_seconds": 150, "starting_id": 50047, "parity": False},
    "5k_hr_h0_sep13_628": {"num_receivers": 100, "msg_rate": 5000, "total_seconds": 150, "starting_id": 50047, "parity": False},
    "100r_10k_120s": {"num_receivers": 100, "msg_rate": 10000, "total_seconds": 120, "starting_id": 3602054, "parity": False},
    "100r_10k_120s_dec9_114": {"num_receivers": 100, "msg_rate": 10000, "total_seconds": 120, "starting_id": 3602054, "parity": False},
    "h0_100r_10k_120s_dec11_1233": {"num_receivers": 100, "msg_rate": 10000, "total_seconds": 120, "starting_id": 3602054, "parity": False},
    "h0_100r_10k_120s_dec11_1237": {"num_receivers": 100, "msg_rate": 10000, "total_seconds": 120, "starting_id": 3602054, "parity": False},
    "h2_100r_dec11_224": {"num_receivers": 100, "msg_rate": 10000, "total_seconds": 120, "starting_id": 3602054, "parity": False},
    "h2_100r_dec11_229": {"num_receivers": 100, "msg_rate": 10000, "total_seconds": 120, "starting_id": 3602054, "parity": False},
    "h0_100r_dec11_251": {"num_receivers": 100, "msg_rate": 10000, "total_seconds": 120, "starting_id": 3602054, "parity": False},
    "h0_100r_dec11_255": {"num_receivers": 100, "msg_rate": 10000, "total_seconds": 120, "starting_id": 3602054, "parity": False},
    "1000r_5k_hr_h2_dec13_413": {"num_receivers": 1000, "msg_rate": 5000, "total_seconds": 120, "starting_id": 3602054, "parity": False},
    "1000r_5k_hr_h2_dec13_424": {"num_receivers": 1000, "msg_rate": 5000, "total_seconds": 120, "starting_id": 3602054, "parity": False},
    "1000r_50k_hr_h2_dec13_430": {"num_receivers": 1000, "msg_rate": 5000, "total_seconds": 120, "starting_id": 3602054, "parity": False},
}
