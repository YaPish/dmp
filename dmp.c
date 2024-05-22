#include <linux/init.h>
#include <linux/module.h>
#include <linux/device-mapper.h>
#include <linux/malloc.h>



typedef struct dmp_stat dmp_stat_t;

/* struct dmp_stat stores statistics of read/write operations
 * Param:
 *  dmp_st_rdcnt: number of reads
 *  dmp_st_rdcnt: number of writes
 *  dmp_st_rdsize: total size of read blocks
 *  dmp_st_wrsize: total size of written blocks
 */
struct dmp_stat {
    size_t dmp_st_rdcnt;
    size_t dmp_st_wrcnt;
    size_t dmp_st_rdsize;
    size_t dmp_st_wrsize;
};



static int dmp_ctr( struct dm_target * ti, unsigned int argc, char ** argv ) {
    dmp_stat_t * dmp_stat = kmalloc( sizeof( dmp_stat_t ), GFP_KERNEL );
    if( !dmp_stat ) {
        ( void )printk( KERN_CRIT "dmp_stat is null\n" );
        return -ENOMEM;
    }

    dmp_stat->dmp_st_rdcnt  = 0;
    dmp_stat->dmp_st_wrcnt  = 0;
    dmp_stat->dmp_st_rdsize = 0;
    dmp_stat->dmp_st_wrsize = 0;

    ti->private = dmp_stat;

    return 0;
}

static void dmp_dtr( struct dm_target *ti ) {
    kfree( ti->private );
}

static int dmp_map( struct dm_target *ti, struct bio *bio ) {
    dmp_stat_t * stat = ( dmp_stat_t * )ti->private;
    if ( bio_data_dir( bio ) == READ ) {
        stat->dmp_st_rdcnt++;
        stat->dmp_st_rdsize += bio->bi_iter.bi_size;
    } else {
        stat->dmp_st_wrcnt++;
        stat->dmp_st_wrsize += bio->bi_iter.bi_size;
    }

    return DM_MAPIO_REMAPPED;
}



static ssize_t dmp_stat( struct kobject *kobj, struct kobj_attribute *attr, char *buf ) {
    dmp_stat_t * stat = container_of( kobj, dmp_stat_t, kobj );
    ssize_t count = 0;

    count += sprintf( buf, "read:\n  reqs: %lu\n  avg size: %zu\n",
            stat->dmp_st_rdcnt, stat->dmp_st_rdsize / stat->dmp_st_rdcnt );
    count += sprintf( buf + count, "write:\n  reqs: %lu\n  avg size: %zu\n",
            stat->dmp_st_wrcnt, stat->dmp_st_wrsize / stat->dmp_st_wrcnt );
    count += sprintf( buf + count, "total:\n  reqs: %lu\n  avg size: %zu\n",
            stat->dmp_st_wrcnt + stat->dmp_st_rdcnt,
            ( stat->dmp_st_wrsize + stat->dmp_st_rdsize ) /
            ( stat->dmp_st_wrcnt  + stat->dmp_st_rdcnt ) );

    return count;
}

static struct kobj_attribute dmp_stat_attr = __ATTR( stat, 0444, dmp_stat, NULL );

static struct attribute * dmp_attrs[] = {
    &dmp_stat_attr.attr,
    NULL,
};

static struct attribute_group dmp_attr_group = {
    .attrs = dmp_attrs,
};

static struct kobject * dmp_kobj;



static struct target_type dmp_target = {
    .name    = "dmp_target",
    .version = { 1, 0, 0 },
    .module  = THIS_MODULE,
    .ctr     = dmp_ctr,
    .dtr     = dmp_dtr,
    .map     = dmp_map,
};

static int __init dmp_init( void ) {
    int result = dm_register_target( &dmp_target );
    if ( result < 0 ) {
        ( void )printk( KERN_ERR "Error in registering DMP target\n" );
        return result;
    }

    dmp_kobj = kobject_create_and_add( "stat", kernel_kobj );
    if ( !dmp_kobj )
        return -ENOMEM;

    if( sysfs_create_group( dmp_kobj, &dmp_attr_group ) )
        goto err;

    ( void )printk( KERN_INFO "DMP module loaded\n" );
    return 0;

err:
    kobject_put( dmp_kobj );
    return -ENOMEM;
}

static void __exit dmp_exit( void ) {
    dm_unregister_target( &dmp_target );
    kobject_put( dmp_kobj );
    ( void )printk( KERN_INFO "DMP module unloaded\n" );
}



module_init( dmp_init );
module_exit( dmp_exit );

MODULE_LICENSE( "GPL" );


