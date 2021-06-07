#include <inc/lib.h>

//Тестирование снимоков файловой системы

union Fsipc fsipcbuf __attribute__((aligned(PGSIZE)));

void
umain(int argc, char **argv) {
    char buf[3];
    int fd;
    
    envid_t fsenv = ipc_find_env(ENV_TYPE_FS);

    //full snapshot test

    fd = open("test", O_CREAT | O_TRUNC | O_WRONLY);
    write(fd,"1",2);
    close(fd);
    strcpy(fsipcbuf.file_snapshot.cmd, "snapshot -f -c snap1");
    ipc_send(fsenv, FSREQ_SNPSHT, &fsipcbuf, PTE_P | PTE_W | PTE_U);
    ipc_recv(NULL, &fsipcbuf,NULL);

    fd = open("test", O_CREAT | O_TRUNC | O_WRONLY);
    write(fd,"2",2);
    close(fd);
    strcpy(fsipcbuf.file_snapshot.cmd, "snapshot -f -c snap2");
    ipc_send(fsenv, FSREQ_SNPSHT, &fsipcbuf, PTE_P | PTE_W | PTE_U);
    ipc_recv(NULL, &fsipcbuf,NULL);

    fd = open("test", O_CREAT | O_TRUNC | O_WRONLY);
    write(fd,"3",2);
    close(fd);
    strcpy(fsipcbuf.file_snapshot.cmd, "snapshot -f -c snap3");
    ipc_send(fsenv, FSREQ_SNPSHT, &fsipcbuf, PTE_P | PTE_W | PTE_U);
    ipc_recv(NULL, &fsipcbuf,NULL);

    fd = open("test", O_CREAT | O_TRUNC | O_WRONLY);
    write(fd,"4",2);
    close(fd);
    strcpy(fsipcbuf.file_snapshot.cmd, "snapshot -f -c snap4");
    ipc_send(fsenv, FSREQ_SNPSHT, &fsipcbuf, PTE_P | PTE_W | PTE_U);
    ipc_recv(NULL, &fsipcbuf,NULL);

    fd = open("test", O_CREAT | O_TRUNC | O_WRONLY);
    write(fd,"5",2);
    close(fd);
    strcpy(fsipcbuf.file_snapshot.cmd, "snapshot -f -c snap5");
    ipc_send(fsenv, FSREQ_SNPSHT, &fsipcbuf, PTE_P | PTE_W | PTE_U);
    ipc_recv(NULL, &fsipcbuf,NULL);

    fd = open("test", O_RDONLY);
    read(fd,buf,2);
    assert(!strcmp(buf,"5"));
    close(fd);

    strcpy(fsipcbuf.file_snapshot.cmd, "snapshot -f -a snap3");
    ipc_send(fsenv, FSREQ_SNPSHT, &fsipcbuf, PTE_P | PTE_W | PTE_U);
    ipc_recv(NULL, &fsipcbuf,NULL);
    fd = open("test", O_RDONLY);
    read(fd,buf,2);
    assert(!strcmp(buf,"3"));
    close(fd);

    strcpy(fsipcbuf.file_snapshot.cmd, "snapshot -f -a snap2");
    ipc_send(fsenv, FSREQ_SNPSHT, &fsipcbuf, PTE_P | PTE_W | PTE_U);
    ipc_recv(NULL, &fsipcbuf,NULL);
    fd = open("test", O_RDONLY);
    read(fd,buf,2);
    assert(!strcmp(buf,"2"));
    close(fd);

    strcpy(fsipcbuf.file_snapshot.cmd, "snapshot -f -a snap3");
    ipc_send(fsenv, FSREQ_SNPSHT, &fsipcbuf, PTE_P | PTE_W | PTE_U);
    ipc_recv(NULL, &fsipcbuf,NULL);
    fd = open("test", O_RDONLY);
    read(fd,buf,2);
    assert(!strcmp(buf,"3"));
    close(fd);

    strcpy(fsipcbuf.file_snapshot.cmd, "snapshot -f -c snap7");
    ipc_send(fsenv, FSREQ_SNPSHT, &fsipcbuf, PTE_P | PTE_W | PTE_U);
    ipc_recv(NULL, &fsipcbuf,NULL);
    fd = open("test", O_CREAT | O_TRUNC | O_WRONLY);
    write(fd,"7",2);
    close(fd);

    strcpy(fsipcbuf.file_snapshot.cmd, "snapshot -f -a snap7");
    ipc_send(fsenv, FSREQ_SNPSHT, &fsipcbuf, PTE_P | PTE_W | PTE_U);
    ipc_recv(NULL, &fsipcbuf,NULL);
    fd = open("test", O_RDONLY);
    read(fd,buf,2);
    assert(!strcmp(buf,"3"));
    close(fd);

    cprintf("Full snapshots work good.\n");


    //delete all  full snapshots
    strcpy(fsipcbuf.file_snapshot.cmd, "snapshot -f -d snap7");
    ipc_send(fsenv, FSREQ_SNPSHT, &fsipcbuf, PTE_P | PTE_W | PTE_U);
    ipc_recv(NULL, &fsipcbuf,NULL);

    strcpy(fsipcbuf.file_snapshot.cmd, "snapshot -f -d snap5");
    ipc_send(fsenv, FSREQ_SNPSHT, &fsipcbuf, PTE_P | PTE_W | PTE_U);
    ipc_recv(NULL, &fsipcbuf,NULL);

    strcpy(fsipcbuf.file_snapshot.cmd, "snapshot -f -d snap4");
    ipc_send(fsenv, FSREQ_SNPSHT, &fsipcbuf, PTE_P | PTE_W | PTE_U);
    ipc_recv(NULL, &fsipcbuf,NULL);

    strcpy(fsipcbuf.file_snapshot.cmd, "snapshot -f -d snap3");
    ipc_send(fsenv, FSREQ_SNPSHT, &fsipcbuf, PTE_P | PTE_W | PTE_U);
    ipc_recv(NULL, &fsipcbuf,NULL);

    strcpy(fsipcbuf.file_snapshot.cmd, "snapshot -f -d snap2");
    ipc_send(fsenv, FSREQ_SNPSHT, &fsipcbuf, PTE_P | PTE_W | PTE_U);
    ipc_recv(NULL, &fsipcbuf,NULL);

    strcpy(fsipcbuf.file_snapshot.cmd, "snapshot -f -d snap1");
    ipc_send(fsenv, FSREQ_SNPSHT, &fsipcbuf, PTE_P | PTE_W | PTE_U);
    ipc_recv(NULL, &fsipcbuf,NULL);

    
    //test incremental snapshot


    fd = open("test", O_CREAT | O_TRUNC | O_WRONLY);
    write(fd,"1",2);
    close(fd);
    strcpy(fsipcbuf.file_snapshot.cmd, "snapshot -i -c snap1");
    ipc_send(fsenv, FSREQ_SNPSHT, &fsipcbuf, PTE_P | PTE_W | PTE_U);
    ipc_recv(NULL, &fsipcbuf,NULL);

    fd = open("test", O_CREAT | O_TRUNC | O_WRONLY);
    write(fd,"2",2);
    close(fd);
    strcpy(fsipcbuf.file_snapshot.cmd, "snapshot -i -c snap2");
    ipc_send(fsenv, FSREQ_SNPSHT, &fsipcbuf, PTE_P | PTE_W | PTE_U);
    ipc_recv(NULL, &fsipcbuf,NULL);

    fd = open("test", O_CREAT | O_TRUNC | O_WRONLY);
    write(fd,"3",2);
    close(fd);
    strcpy(fsipcbuf.file_snapshot.cmd, "snapshot -i -c snap3");
    ipc_send(fsenv, FSREQ_SNPSHT, &fsipcbuf, PTE_P | PTE_W | PTE_U);
    ipc_recv(NULL, &fsipcbuf,NULL);

    fd = open("test", O_CREAT | O_TRUNC | O_WRONLY);
    write(fd,"4",2);
    close(fd);
    strcpy(fsipcbuf.file_snapshot.cmd, "snapshot -i -c snap4");
    ipc_send(fsenv, FSREQ_SNPSHT, &fsipcbuf, PTE_P | PTE_W | PTE_U);
    ipc_recv(NULL, &fsipcbuf,NULL);

    fd = open("test", O_CREAT | O_TRUNC | O_WRONLY);
    write(fd,"5",2);
    close(fd);
    strcpy(fsipcbuf.file_snapshot.cmd, "snapshot -i -c snap5");
    ipc_send(fsenv, FSREQ_SNPSHT, &fsipcbuf, PTE_P | PTE_W | PTE_U);
    ipc_recv(NULL, &fsipcbuf,NULL);


    strcpy(fsipcbuf.file_snapshot.cmd, "snapshot -i -a snap3");
    ipc_send(fsenv, FSREQ_SNPSHT, &fsipcbuf, PTE_P | PTE_W | PTE_U);
    ipc_recv(NULL, &fsipcbuf,NULL);
    fd = open("test", O_RDONLY);
    read(fd,buf,2);
    assert(!strcmp(buf,"3"));
    close(fd);

    strcpy(fsipcbuf.file_snapshot.cmd, "snapshot -i -e");
    ipc_send(fsenv, FSREQ_SNPSHT, &fsipcbuf, PTE_P | PTE_W | PTE_U);
    ipc_recv(NULL, &fsipcbuf,NULL);
    strcpy(fsipcbuf.file_snapshot.cmd, "snapshot -i -a snap4");
    ipc_send(fsenv, FSREQ_SNPSHT, &fsipcbuf, PTE_P | PTE_W | PTE_U);
    ipc_recv(NULL, &fsipcbuf,NULL);
    fd = open("test", O_RDONLY);
    read(fd,buf,2);
    assert(!strcmp(buf,"4"));
    close(fd);


    strcpy(fsipcbuf.file_snapshot.cmd, "snapshot -i -e");
    ipc_send(fsenv, FSREQ_SNPSHT, &fsipcbuf, PTE_P | PTE_W | PTE_U);
    ipc_recv(NULL, &fsipcbuf,NULL);
    strcpy(fsipcbuf.file_snapshot.cmd, "snapshot -i -a snap4");
    ipc_send(fsenv, FSREQ_SNPSHT, &fsipcbuf, PTE_P | PTE_W | PTE_U);
    ipc_recv(NULL, &fsipcbuf,NULL);
    fd = open("test", O_RDONLY);
    read(fd,buf,2);
    assert(!strcmp(buf,"4"));
    close(fd);

    strcpy(fsipcbuf.file_snapshot.cmd, "snapshot -i -e");
    ipc_send(fsenv, FSREQ_SNPSHT, &fsipcbuf, PTE_P | PTE_W | PTE_U);
    ipc_recv(NULL, &fsipcbuf,NULL);
    strcpy(fsipcbuf.file_snapshot.cmd, "snapshot -i -a snap5");
    ipc_send(fsenv, FSREQ_SNPSHT, &fsipcbuf, PTE_P | PTE_W | PTE_U);
    ipc_recv(NULL, &fsipcbuf,NULL);
    fd = open("test", O_RDONLY);
    read(fd,buf,2);
    assert(!strcmp(buf,"5"));
    close(fd);

    strcpy(fsipcbuf.file_snapshot.cmd, "snapshot -i -e");
    ipc_send(fsenv, FSREQ_SNPSHT, &fsipcbuf, PTE_P | PTE_W | PTE_U);
    ipc_recv(NULL, &fsipcbuf,NULL);
    strcpy(fsipcbuf.file_snapshot.cmd, "snapshot -i -d snap5");
    ipc_send(fsenv, FSREQ_SNPSHT, &fsipcbuf, PTE_P | PTE_W | PTE_U);
    ipc_recv(NULL, &fsipcbuf,NULL);
    

    cprintf("Incremental snapshots work good.\n");
}