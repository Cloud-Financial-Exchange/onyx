#include <linux/module.h>
#define INCLUDE_VERMAGIC
#include <linux/build-salt.h>
#include <linux/elfnote-lto.h>
#include <linux/export-internal.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

BUILD_SALT;
BUILD_LTO_INFO;

MODULE_INFO(vermagic, VERMAGIC_STRING);
MODULE_INFO(name, KBUILD_MODNAME);

__visible struct module __this_module
__section(".gnu.linkonce.this_module") = {
	.name = KBUILD_MODNAME,
	.init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
	.exit = cleanup_module,
#endif
	.arch = MODULE_ARCH_INIT,
};

#ifdef CONFIG_RETPOLINE
MODULE_INFO(retpoline, "Y");
#endif


static const struct modversion_info ____versions[]
__used __section("__versions") = {
	{ 0xbdfb6dbb, "__fentry__" },
	{ 0x5c3c7387, "kstrtoull" },
	{ 0x92cfd226, "pci_num_vf" },
	{ 0x5b8239ca, "__x86_return_thunk" },
	{ 0x68f4ccdb, "pci_disable_sriov" },
	{ 0x3412cb05, "pci_enable_sriov" },
	{ 0xd0da656b, "__stack_chk_fail" },
	{ 0x65487097, "__x86_indirect_thunk_rax" },
	{ 0x656e4a6e, "snprintf" },
	{ 0x2e4a0e59, "pci_clear_master" },
	{ 0xc1514a3b, "free_irq" },
	{ 0x4b51d682, "pci_free_irq_vectors" },
	{ 0xe41e0f70, "uio_event_notify" },
	{ 0xdab22344, "pci_check_and_mask_intx" },
	{ 0xf038b3d, "pci_set_master" },
	{ 0x92d5838e, "request_threaded_irq" },
	{ 0xe1613e67, "_dev_info" },
	{ 0x3f629d74, "pci_alloc_irq_vectors_affinity" },
	{ 0xfac191bc, "pci_irq_vector" },
	{ 0x84fd8e21, "__dynamic_dev_dbg" },
	{ 0x8c36cd28, "_dev_err" },
	{ 0xf5f3432f, "_dev_notice" },
	{ 0xe0d5c691, "irq_get_irq_data" },
	{ 0x64fa5afc, "pci_cfg_access_lock" },
	{ 0xa870fce4, "pci_cfg_access_unlock" },
	{ 0x4d4fdde8, "pci_msi_mask_irq" },
	{ 0xe4c56fec, "pci_intx" },
	{ 0x2b7398bd, "pci_msi_unmask_irq" },
	{ 0x63d5e7de, "kmalloc_caches" },
	{ 0xad639449, "kmem_cache_alloc_trace" },
	{ 0x114dce5d, "pci_enable_device" },
	{ 0x6d00e36b, "dma_set_mask" },
	{ 0xc9d8645a, "dma_set_coherent_mask" },
	{ 0x6683817a, "sysfs_create_group" },
	{ 0x5bbc484f, "__uio_register_device" },
	{ 0xabaee757, "dma_alloc_attrs" },
	{ 0xcf93f8d0, "dma_free_attrs" },
	{ 0x1b1d9842, "sysfs_remove_group" },
	{ 0xedc03953, "iounmap" },
	{ 0x1d8d3706, "pci_disable_device" },
	{ 0x37a0cba, "kfree" },
	{ 0xde80cd09, "ioremap" },
	{ 0x87a21cb3, "__ubsan_handle_out_of_bounds" },
	{ 0x579e57b8, "_dev_warn" },
	{ 0xd994a662, "uio_unregister_device" },
	{ 0x92997ed8, "_printk" },
	{ 0xe2d5255a, "strcmp" },
	{ 0x14b0bccb, "__pci_register_driver" },
	{ 0x165d53f8, "pci_unregister_driver" },
	{ 0xf2f88b7f, "param_ops_int" },
	{ 0xa1610d1d, "param_ops_charp" },
	{ 0x1bdfd280, "module_layout" },
};

MODULE_INFO(depends, "uio");


MODULE_INFO(srcversion, "477CD0B44C1D25E57128F06");
