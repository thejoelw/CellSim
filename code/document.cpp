#include "document.h"
#include "simview.h"
#include "globals.h"

#include "math.h"

#include <QFileDialog>
#include <QFile>
#include <QString>
#include <QDebug>

Document::Document(QWidget *parent) :
    QMdiArea(parent)
{
    sim = new Simulation(this);
    addSubWindow(new SimView(sim, this));
}

void Document::open()
{
    fileDialog.setFileMode(QFileDialog::ExistingFiles);
    if (fileDialog.exec())
    {
        QStringList files = fileDialog.selectedFiles();
        openFile(files.first());
        int i = 1;
        int c = files.length();
        while (i < c)
        {
            Globals::mw->openDoc(files[i]);
            i++;
        }
    }
}

void Document::save()
{
    if (bridge.fileName().isEmpty())
    {
        fileDialog.setFileMode(QFileDialog::AnyFile);
        if (fileDialog.exec())
        {
            bridge.setFileName(fileDialog.selectedFiles().first());
        }
    }

    bridge.open(QFile::WriteOnly);
    bridge.write(encodeSim());
    bridge.close();
}

void Document::saveAs()
{

}

void Document::openFile(QString path)
{
    bridge.setFileName(path);
    bridge.open(QFile::ReadOnly);
    decodeSim(bridge.readAll());
    bridge.close();
}

QByteArray Document::encodeSim()
{
    QByteArray str = QByteArray();

    char atomStr[64];
    int j = 0;
    while (j < sim->numAtoms)
    {
        int len = encodeAtom(sim->atoms[j], atomStr);
        str.append(atomStr, len);
        j++;
    }

    QByteArray comp = qCompress(str, 9);

    return comp;
}

void Document::decodeSim(QByteArray str)
{
    int len = str.length();
    const char* cStr = (str + QString(64, ' ')).constData();
    const char* cStrEnd = cStr + len;

}

int Document::encodeAtom(Atom* a, char* str)
{
    // Selected * type (1 char)
    // nx (4 chars)
    // ny (4 chars)
    // vx (4 chars)
    // vy (4 chars)
    // state (2 chars)
    // reactionStr (variable length)
    // bondsLt (variable length)

    char* strStart = str;

    *(str++) = char(a->element * 2 + a->selected);

    float nx = float(a->nx);
    memcpy(str += 4, &nx, 4);
    float ny = float(a->ny);
    memcpy(str += 4, &ny, 4);

    float vx = float(a->vx);
    memcpy(str += 4, &vx, 4);
    float vy = float(a->vy);
    memcpy(str += 4, &vy, 4);

    memcpy(str += 2, &a->state, 2);

    int len = a->reactionStr.length();
    *(str++) = char(len);
    memcpy(str += len, &a->reactionStr, len);

    int maxBond = 0;
    int j = 0;
    while (j < a->numBondsLt)
    {
        int bond = a->bondsLt[j]->i;
        if (bond > maxBond) { maxBond = bond; }
        j++;
    }

    int bondSize = int(log(maxBond) / 2.40823);
    *(str++) = char(a->numBondsLt * 4 + bondSize);
    j = 0;
    while (j < a->numBondsLt)
    {
        memcpy(str += bondSize, &a->bondsLt[j], bondSize);
    }

    qDebug() << str - strStart;
    qDebug() << 21 + len + bondSize * a->numBondsLt;
    return 21 + len + bondSize * a->numBondsLt;
}

void Document::decodeAtom(char* str)
{
    bool selected = bool(*str % 2);
    int element = int(*(str++) / 2);

    float nx;
    memcpy(&nx, str += 4, 4);
    float ny;
    memcpy(&ny, str += 4, 4);

    float vx;
    memcpy(&vx, str += 4, 4);
    float vy;
    memcpy(&vy, str += 4, 4);

    int state;
    memcpy(&state, str += 2, 2);

    int maxBond = sim->numAtoms - 1;

    Atom* a = sim->addAtom(QPointF(qreal(nx), ny), element, state);

    a->vx = vx;
    a->vy = vy;
    if (selected) { a->select(); }

    int len = int(*(str++));
    Globals::ae->reactionStrToArr(QString(QByteArray(str += len, len)), a->reaction);

    int numBonds = int(*str / 4);
    int lenBonds = *(str++) % 4;
    int i = 0;
    while (i < numBonds)
    {
        int bi;
        memcpy(&bi, str += lenBonds, lenBonds);

        if (bi > maxBond) {return;}
        Atom* b = sim->atoms[bi];
        if (a->numBondsLt < 6 && b->numBondsGt < 6)
        {
            a->bondsLt[a->numBondsLt++] = b;
            b->bondsGt[b->numBondsGt++] = a;
        }
    }
}
