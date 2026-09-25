/* vgr3_play.c -- see vgr3_play.h. No libc calls. */
#include "vgr3_play.h"

static uint16_t rd16(const uint8_t *p) {
    return (uint16_t)(p[0] | (p[1] << 8));
}

static void put(Vgr3Player *p, uint8_t r, uint8_t v) {
    p->regs[r] = v;
    p->dirty[r >> 3] |= (uint8_t)(1 << (r & 7));
}

/* Executes a SET whose opcode is `op` and whose operands start at q.
 * Returns the address after the operands. */
static const uint8_t *doSet(Vgr3Player *p, Vgr3Chan *c, uint8_t op, const uint8_t *q) {
    uint8_t mask = (uint8_t)((op & 0x7F) >> c->k);
    uint8_t wait = (uint8_t)(op & ((1 << c->k) - 1));
    uint8_t r = c->base;
    while (mask) {
        if (mask & 1) put(p, r, *q++);
        mask >>= 1;
        r++;
    }
    if (!wait) wait = *q++;
    c->wait = wait;
    return q;
}

static void call(Vgr3Chan *c, const uint8_t *target, uint8_t count) {
    Vgr3Frame *f = &c->stack[c->sp++];
    f->ret = c->pc;
    f->left = count;
    c->pc = target;
}

/* Runs one item. */
static void step(Vgr3Player *p, Vgr3Chan *c) {
    const uint8_t *at;
    uint8_t op;
    while (c->sp && c->stack[c->sp - 1].left == 0)
        c->pc = c->stack[--c->sp].ret;
    at = c->pc;
    op = *c->pc++;
    if (c->sp) c->stack[c->sp - 1].left--;

    if (op & 0x80) {
        c->pc = doSet(p, c, op, c->pc);
    } else if (op < VGR3_OP_DICT_END) {
        /* the entry is a SET, or a CALLM/CALLL (returns to after this op) */
        const uint8_t *q = p->data + rd16(p->dict + op * 2);
        if (q[0] & 0x80) doSet(p, c, q[0], q + 1);
        else call(c, p->data + rd16(q + 1), (uint8_t)(q[0] == VGR3_OP_CALLL ? q[3] : (q[0] & 15) + 1));
    } else if (op >= VGR3_OP_CALLM) {
        const uint8_t *t = p->data + rd16(c->pc);
        c->pc += 2;
        call(c, t, (uint8_t)((op & 15) + 1));
    } else if (op >= VGR3_OP_CALLS) {
        const uint8_t *t = at - *c->pc++;
        call(c, t, (uint8_t)((op & 15) + 1));
    } else if (op == VGR3_OP_CALLL) {
        const uint8_t *t = p->data + rd16(c->pc);
        uint8_t n = c->pc[2];
        c->pc += 3;
        call(c, t, n);
    } else if (op == VGR3_OP_JUMP) {
        c->pc = p->data + rd16(c->pc);
    } else {
#if VGR3_FEAT_EXT
        if (op == VGR3_OP_EXT) {
            uint8_t sub = *c->pc++;
            if (sub == VGR3_EXT_LOAD) {
                uint8_t i;
                for (i = 0; i < c->width; i++) put(p, (uint8_t)(c->base + i), *c->pc++);
                return;
            }
        }
#endif
        /* END or unknown: hold */
        c->pc = at;
        c->wait = 255;
    }
}

int vgr3Init(Vgr3Player *p, const uint8_t *file) {
    const uint8_t *ct;
    uint8_t i, nd;
    if (file[0] != 'V' || file[1] != 'G' || file[2] != 'R' || file[3] != '3' ||
        file[4] != VGR3_VERSION || file[7] > VGR3_MAX_CHANS)
        return 0;
    p->numChans = file[7];
    nd = file[8];
    ct = file + VGR3_HEADER_SIZE;
    p->dict = ct + p->numChans * VGR3_CHAN_SIZE;
    p->data = p->dict + nd * 2;
    for (i = 0; i < VGR3_MAX_REGS; i++) p->regs[i] = 0;
    for (i = 0; i < VGR3_MAX_REGS / 8; i++) p->dirty[i] = 0;
    for (i = 0; i < p->numChans; i++, ct += VGR3_CHAN_SIZE) {
        Vgr3Chan *c = &p->chans[i];
        c->base = ct[0];
        c->width = ct[1];
        c->k = (uint8_t)(c->width <= VGR3_MASK_MAX_W ? 7 - c->width : 7);
        c->pc = p->data + rd16(ct + 2);
        c->wait = 0;
        c->sp = 0;
    }
    return 1;
}

void vgr3Frame(Vgr3Player *p) {
    uint8_t i;
    for (i = 0; i < p->numChans; i++) {
        Vgr3Chan *c = &p->chans[i];
        while (c->wait == 0) step(p, c);
        c->wait--;
    }
}
