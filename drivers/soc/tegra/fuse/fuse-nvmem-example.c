// SPDX-License-Identifier: GPL-2.0
#include <linux/module.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/nvmem-consumer.h>
#include <linux/nvmem-provider.h>
#include <linux/string.h>
#include <linux/slab.h>

static char *providers[] = { "fuse0", "fuse1", "fuse2" };
static int num_providers = 3;
module_param_array(providers, charp, &num_providers, 0444);
MODULE_PARM_DESC(providers, "Provider names: e.g., fuse0,fuse1,fuse2");

static unsigned int offsets[3] = { 0, 0, 0 };
static int num_offsets;
module_param_array(offsets, uint, &num_offsets, 0444);
MODULE_PARM_DESC(offsets, "Read offsets for each provider");

static unsigned int lens[3] = { 16, 16, 16 };
static int num_lens;
module_param_array(lens, uint, &num_lens, 0444);
MODULE_PARM_DESC(lens, "Read lengths for each provider");

/* This example is provider-centric only; no consumer (lookup) path. */

/*
 * We intentionally avoid nvmem_device_get() here because it requires a
 * consumer struct device. This example is a standalone module, so we use the
 * provider-centric API nvmem_device_find(). It iterates all registered NVMEM
 * providers and invokes a match callback with each provider's struct device.
 * We match by the provider's device name (e.g., "fuse0", "fuse1", "fuse2"),
 * which is the full provider name returned by nvmem_dev_name().
 */

static void hexdump_line(const u8 *buf, size_t len)
{
	size_t i;
	for (i = 0; i < len; i++)
		pr_info("%02x%s", buf[i], (i + 1 == len) ? "" : " ");
}

static int match_nvmem_name(struct device *dev, const void *data)
{
	const char *name = data;
	const char *dn = dev_name(dev);
	return dn && !strcmp(dn, name);
}

static int read_from_provider(const char *prov, unsigned int off, unsigned int len)
{
	struct nvmem_device *nvmem;
	u8 *buf;
	int ret;

	/* Find the NVMEM provider whose device name equals 'prov' (e.g., "fuse1"). */
	nvmem = nvmem_device_find((void *)prov, match_nvmem_name);
	if (IS_ERR(nvmem)) {
		ret = PTR_ERR(nvmem);
		pr_err("nvmem_device_get(%s) failed: %d\n", prov, ret);
		return ret;
	}
	if (!nvmem) {
		pr_err("nvmem provider '%s' not found\n", prov);
		return -ENODEV;
	}

	pr_info("provider '%s' found (dev=%s, size=%zu)\n",
		prov, nvmem_dev_name(nvmem), nvmem_dev_size(nvmem));

	if (!len) {
		pr_info("skip read from '%s': len=0 (off=0x%x)\n", prov, off);
		nvmem_device_put(nvmem);
		return 0;
	}

	buf = kzalloc(len, GFP_KERNEL);
	if (!buf) {
		nvmem_device_put(nvmem);
		return -ENOMEM;
	}

	pr_info("reading %u bytes from '%s' at off=0x%x\n", len, prov, off);
	ret = nvmem_device_read(nvmem, off, len, buf);
	if (ret) {
		pr_err("read %s off=0x%x len=%u failed: %d\n", prov, off, len, ret);
		goto out;
	}

	pr_info("nvmem %s [0x%x..0x%x): ", prov, off, off + len);
	hexdump_line(buf, len);
	pr_cont("\n");

out:
	kfree(buf);
	nvmem_device_put(nvmem);
	return ret;
}

static int __init fuse_nvmem_example_init(void)
{
	int i;
        int ret;

        pr_info("fuse-nvmem-example: start\n");
	for (i = 0; i < num_providers; i++) {
		unsigned int off = (i < num_offsets) ? offsets[i] : 0;
		unsigned int len = (i < num_lens) ? lens[i] : 0;
		if (providers[i]) {
			ret = read_from_provider(providers[i], off, len);
			if (!ret)
				pr_info("fuse-nvmem-example: done provider '%s'\n", providers[i]);
		}
	}

        pr_info("fuse-nvmem-example: complete\n");

	return 0;
}

static void __exit fuse_nvmem_example_exit(void)
{
}

module_init(fuse_nvmem_example_init);
module_exit(fuse_nvmem_example_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Example module to read Tegra fuse nvmem (providers and lookups)");
MODULE_AUTHOR("example");


