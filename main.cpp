#define _USE_MATH_DEFINES
#include <cmath>
#include <wx/wx.h>
#include <wx/timer.h>
#include <wx/colordlg.h>
#include <wx/colour.h>
#include <wx/graphics.h>
#include <wx/dcbuffer.h>
#include <vector>
#include <ctime>
#include <algorithm>
#include <fstream>
#include <map>
#include <clocale>
#include <cstring>

// Хелпер для корректного отображения кириллицы в wxWidgets
static wxString ru(const char* s) { return wxString::FromUTF8(s); }

using namespace std;

// рекорд поиск и запись
static const string SCORE_FILE = "scores.txt";

static int LoadHighScore(const string& game) {
    ifstream in(SCORE_FILE);
    string g; int s;
    while (in >> g >> s)
        if (g == game) return s;
    return 0;
}

static void SaveHighScore(const string& game, int score) {
    map<string,int> records;
    { ifstream in(SCORE_FILE); string g; int s;
      while (in >> g >> s) records[g] = s; }
    records[game] = score;
    ofstream out(SCORE_FILE);
    for (auto it=records.begin(); it!=records.end(); ++it)
        out << it->first << " " << it->second << "\n";
}

// прототип функций
// Объявление класса SecondFrame
class SecondFrame;

// окно разроботки
// Класс StubGame наследует wxFrame
class StubGame : public wxFrame {
public:
    // Конструктор с инициализатором
    StubGame(const wxString& title) :
        wxFrame(nullptr, wxID_ANY, title, wxDefaultPosition, wxSize(800, 600))
    {
        wxPanel* panel = new wxPanel(this);
        // Установка цвета фона
        panel->SetBackgroundColour(wxColour(30, 30, 30));
        wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);

        wxStaticText* lbl = new wxStaticText(panel, wxID_ANY, title,
            wxDefaultPosition, wxDefaultSize, wxALIGN_CENTER);
        // Установка цвета текста
        lbl->SetForegroundColour(*wxWHITE);
        lbl->SetFont(wxFont(28, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD));

        wxStaticText* sub = new wxStaticText(panel, wxID_ANY, ru("В разработке..."), //подзогололок
            wxDefaultPosition, wxDefaultSize, wxALIGN_CENTER);
        // Установка цвета текста
        lbl->SetForegroundColour(wxColour(180, 180, 180));
        sub->SetFont(wxFont(16, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_ITALIC, wxFONTWEIGHT_NORMAL));

        wxButton* backBtn = new wxButton(panel, wxID_ANY, ru("В меню"),
            wxDefaultPosition, wxSize(150, 40));
        backBtn->Bind(wxEVT_BUTTON, &StubGame::OnBack, this); //событие

        sizer->AddStretchSpacer(1); //расположение
        sizer->Add(lbl,     0, wxALIGN_CENTER | wxALL, 10);
        sizer->Add(sub,     0, wxALIGN_CENTER | wxALL, 5);
        sizer->AddSpacer(30); //пустые блоки
        sizer->Add(backBtn, 0, wxALIGN_CENTER | wxALL, 10);
        sizer->AddStretchSpacer(1);
        panel->SetSizer(sizer);
    }

    void OnBack(wxCommandEvent&); //прототип
};
////игры в разроботке
// ─── Игра на память (Memory) ──────────────────────────────────────────────────
// Класс gamememory наследует wxFrame
class gamememory : public wxFrame {
    struct DiffConfig { int cols, rows; wxString name; };
    static DiffConfig DIFFS[3];
    int diffIndex = 1; // 0=Лёгко  1=Нормально  2=Сложно

    int COLS=4, ROWS=4;
    int total() const { return COLS*ROWS; }

    struct Card {
        int   value   = 0;
        bool  flipped = false;
        bool  matched = false;
        float anim    = 0.f;  // 0=рубашка, 1=лицо
    };

    // Динамический массив карточек
    vector<Card> cards;
    int   first = -1, second = -1;
    int   moves = 0, matchedPairs = 0, bestMoves = 0;
    bool  waiting = false;
    wxPanel*  panel;
    wxTimer   flipTimer;
    wxTimer   animTimer;
    wxButton* diffBtns[3] = {};
    int cellW=120, cellH=100, gx=0, gy=0;

    static wxColour cardColor(int v) {
        static const wxColour C[] = {
            wxColour(220,60,60),  wxColour(60,130,220), wxColour(60,185,90),
            wxColour(220,190,50), wxColour(170,65,210), wxColour(220,130,50),
            wxColour(50,200,200), wxColour(220,80,160),
            // дополнительные цвета для сложного уровня
            wxColour(0,200,150),  wxColour(200,60,200),
            wxColour(100,210,50), wxColour(240,100,40),
        };
        return C[v % 12];
    }

    // Вычисление позиций элементов интерфейса
    void computeLayout() {
        wxSize panelSz = panel->GetSize();
        int panelW=panelSz.GetWidth(), panelH =panelSz.GetHeight();
        cellW = min((panelW-40)/COLS, (panelH-100)/ROWS);
        cellH = cellW * 85 / 100;
        gx = (panelW - cellW*COLS)/2;
        gy = 55 + (panelH - 55 - cellH*ROWS)/2;
    }

    // Перемешивание карточек в зависимости от сложности
    void shuffle() {
        COLS = DIFFS[diffIndex].cols;
        ROWS = DIFFS[diffIndex].rows;
        int n = total();
        vector<int> cardValues(n);
        for (int i=0;i<n/2;i++) cardValues[i]=cardValues[i+n/2]=i;
        for (int i=n-1;i>0;i--) swap(cardValues[i],cardValues[rand()%(i+1)]);
        cards.resize(n);
        for (int i=0;i<n;i++) cards[i]={cardValues[i],false,false,0.f};
        first=second=-1; moves=0; matchedPairs=0; waiting=false;
        string scoreKey="Memory_"+to_string(COLS)+"x"+to_string(ROWS);
        bestMoves=LoadHighScore(scoreKey);
    }

    void onFlipTimer(wxTimerEvent&) {
        flipTimer.Stop();
        if (first>=0&&second>=0&&cards[first].value!=cards[second].value) {
            cards[first].flipped = cards[second].flipped = false;
        }
        first=second=-1; waiting=false;
        if (!animTimer.IsRunning()) animTimer.Start(16);
    }

    void onAnimTimer(wxTimerEvent&) {
        bool any=false;
        for (auto& c : cards) {
            float target=(c.flipped||c.matched)?1.f:0.f;
            if (abs(c.anim-target)>0.02f) {
                c.anim += (target-c.anim)*0.22f;
                any=true;
            } else c.anim=target;
        }
        panel->Refresh();
        if (!any) animTimer.Stop();
    }

    // Рисует символ по центру (centX,centY) с радиусом r
    // Рисование символа на карточке
    void drawSymbol(wxGraphicsContext* gctx, int type, double centX, double centY, double r) {
        wxColour white(255,255,255,230);
        gctx->SetBrush(wxBrush(white));
        gctx->SetPen(gctx->CreatePen(wxGraphicsPenInfo(wxColour(255,255,255,180),r*0.12).Cap(wxCAP_ROUND).Join(wxJOIN_ROUND)));

        auto path = gctx->CreatePath();

        switch (type % 12) {
        case 0: { // Звезда (5 лучей)
            for (int i=0;i<5;i++) {
                double a=i*72-90, ar=(a+36)*M_PI/180, ao=a*M_PI/180;
                double ox=centX+r*cos(ao), oy=centY+r*sin(ao);
                double infoX=centX+r*0.4*cos(ar), iy=centY+r*0.4*sin(ar);
                if (i==0) path.MoveToPoint(ox,oy);
                else       path.AddLineToPoint(ox,oy);
                path.AddLineToPoint(infoX,iy);
            }
            path.CloseSubpath();
            gctx->FillPath(path); gctx->StrokePath(path);
            break; }
        case 1: { // Сердце
            double s=r*0.9;
            path.MoveToPoint(centX, centY+s*0.8);
            path.AddCurveToPoint(centX-s*1.1,centY-s*0.3, centX-s*1.1,centY-s*0.9, centX,centY-s*0.2);
            path.AddCurveToPoint(centX+s*1.1,centY-s*0.9, centX+s*1.1,centY-s*0.3, centX,centY+s*0.8);
            gctx->FillPath(path); gctx->StrokePath(path);
            break; }
        case 2: { // Ромб
            path.MoveToPoint(centX,    centY-r);
            path.AddLineToPoint(centX+r*0.7, centY);
            path.AddLineToPoint(centX,    centY+r);
            path.AddLineToPoint(centX-r*0.7, centY);
            path.CloseSubpath();
            gctx->FillPath(path); gctx->StrokePath(path);
            break; }
        case 3: { // Треугольник
            for (int i=0;i<3;i++) {
                double a=(i*120-90)*M_PI/180;
                double px=centX+r*cos(a), py=centY+r*sin(a);
                if (i==0) path.MoveToPoint(px,py);
                else       path.AddLineToPoint(px,py);
            }
            path.CloseSubpath();
            gctx->FillPath(path); gctx->StrokePath(path);
            break; }
        case 4: { // Луна (crescent)
            path.AddArc(centX,centY,r,0.4,M_PI*2-0.4,true);
            path.AddArc(centX+r*0.45,centY-r*0.1,r*0.78,M_PI*2-0.4,0.4,false);
            gctx->FillPath(path);
            break; }
        case 5: { // Молния
            path.MoveToPoint(centX+r*0.2,  centY-r);
            path.AddLineToPoint(centX-r*0.2, centY-r*0.1);
            path.AddLineToPoint(centX+r*0.15,centY-r*0.1);
            path.AddLineToPoint(centX-r*0.2, centY+r);
            path.AddLineToPoint(centX+r*0.2, centY+r*0.1);
            path.AddLineToPoint(centX-r*0.15,centY+r*0.1);
            path.CloseSubpath();
            gctx->FillPath(path); gctx->StrokePath(path);
            break; }
        case 6: { // Крест
            double t=r*0.32;
            path.MoveToPoint(centX-t,centY-r); path.AddLineToPoint(centX+t,centY-r);
            path.AddLineToPoint(centX+t,centY-t); path.AddLineToPoint(centX+r,centY-t);
            path.AddLineToPoint(centX+r,centY+t); path.AddLineToPoint(centX+t,centY+t);
            path.AddLineToPoint(centX+t,centY+r); path.AddLineToPoint(centX-t,centY+r);
            path.AddLineToPoint(centX-t,centY+t); path.AddLineToPoint(centX-r,centY+t);
            path.AddLineToPoint(centX-r,centY-t); path.AddLineToPoint(centX-t,centY-t);
            path.CloseSubpath();
            gctx->FillPath(path); gctx->StrokePath(path);
            break; }
        case 7: { // Шестиугольник
            for (int i=0;i<6;i++) {
                double a=(i*60-30)*M_PI/180;
                double px=centX+r*cos(a), py=centY+r*sin(a);
                if (i==0) path.MoveToPoint(px,py);
                else       path.AddLineToPoint(px,py);
            }
            path.CloseSubpath();
            gctx->FillPath(path); gctx->StrokePath(path);
            break; }
        case 8: { // Пятиугольник
            for (int i=0;i<5;i++) {
                double a=(i*72-90)*M_PI/180;
                double px=centX+r*cos(a), py=centY+r*sin(a);
                if (i==0) path.MoveToPoint(px,py);
                else       path.AddLineToPoint(px,py);
            }
            path.CloseSubpath();
            gctx->FillPath(path); gctx->StrokePath(path);
            break; }
        case 9: { // Стрелка вверх
            double w=r*0.38;
            path.MoveToPoint(centX,          centY-r);
            path.AddLineToPoint(centX+r*0.8,  centY-r*0.05);
            path.AddLineToPoint(centX+w,      centY-r*0.05);
            path.AddLineToPoint(centX+w,      centY+r);
            path.AddLineToPoint(centX-w,      centY+r);
            path.AddLineToPoint(centX-w,      centY-r*0.05);
            path.AddLineToPoint(centX-r*0.8,  centY-r*0.05);
            path.CloseSubpath();
            gctx->FillPath(path); gctx->StrokePath(path);
            break; }
        case 10: { // Песочные часы
            path.MoveToPoint(centX-r*0.75, centY-r);
            path.AddLineToPoint(centX+r*0.75, centY-r);
            path.AddLineToPoint(centX,         centY);
            path.AddLineToPoint(centX+r*0.75, centY+r);
            path.AddLineToPoint(centX-r*0.75, centY+r);
            path.AddLineToPoint(centX,         centY);
            path.CloseSubpath();
            gctx->FillPath(path); gctx->StrokePath(path);
            break; }
        case 11: { // Восьмиугольник
            for (int i=0;i<8;i++) {
                double a=(i*45)*M_PI/180;
                double px=centX+r*cos(a), py=centY+r*sin(a);
                if (i==0) path.MoveToPoint(px,py);
                else       path.AddLineToPoint(px,py);
            }
            path.CloseSubpath();
            gctx->FillPath(path); gctx->StrokePath(path);
            break; }
        }
    }

    // Рисование карточки
    void drawCard(wxGraphicsContext* gctx, int idx) {
        int col=idx%COLS, row=idx/COLS;
        double x=gx+col*cellW+4, y=gy+row*cellH+4;
        double w=cellW-8, h=cellH-8;
        float  a=cards[idx].anim;
        double scaleX=abs(2.0*a-1.0);
        double centX=x+w/2;
        double drawWidth=w*max(scaleX,0.05);
        bool showFace=(a>=0.5f);

        if (!showFace) {
            gctx->SetBrush(gctx->CreateLinearGradientBrush(centX-drawWidth/2,y,centX+drawWidth/2,y+h,
                wxColour(35,50,100),wxColour(20,32,75)));
            gctx->SetPen(gctx->CreatePen(wxGraphicsPenInfo(wxColour(60,85,160),1.5)));
            gctx->DrawRoundedRectangle(centX-drawWidth/2,y,drawWidth,h,8);
            if (scaleX>0.3) {
                gctx->SetPen(gctx->CreatePen(wxGraphicsPenInfo(wxColour(50,70,130,100),1)));
                for (int i=1;i<4;i++) gctx->StrokeLine(centX-drawWidth/2+drawWidth*i/4,y+4,centX-drawWidth/2+drawWidth*i/4,y+h-4);
            }
        } else {
            wxColour base=cardColor(cards[idx].value);
            if (cards[idx].matched) base=base.ChangeLightness(80);
            gctx->SetBrush(gctx->CreateLinearGradientBrush(centX-drawWidth/2,y,centX-drawWidth/2,y+h,
                base.ChangeLightness(120),base.ChangeLightness(80)));
            gctx->SetPen(cards[idx].matched
                ?gctx->CreatePen(wxGraphicsPenInfo(wxColour(255,255,255,160),2.5))
                :gctx->CreatePen(wxGraphicsPenInfo(base.ChangeLightness(55),1.5)));
            gctx->DrawRoundedRectangle(centX-drawWidth/2,y,drawWidth,h,8);
            if (scaleX>0.45) {
                drawSymbol(gctx, cards[idx].value, centX, y+h/2, h*0.28);
            }
            gctx->SetBrush(gctx->CreateLinearGradientBrush(centX-drawWidth/2,y,centX-drawWidth/2,y+h/3,
                wxColour(255,255,255,55),wxColour(255,255,255,0)));
            gctx->SetPen(*wxTRANSPARENT_PEN);
            gctx->DrawRoundedRectangle(centX-drawWidth/2+2,y+2,drawWidth-4,h/3,5);
        }
    }

    // Отрисовка графики на экране
    void onPaint(wxPaintEvent&) {
        wxAutoBufferedPaintDC dc(panel);
        wxSize panelSz=panel->GetSize();
        int panelW=panelSz.GetWidth(), panelH =panelSz.GetHeight();
        computeLayout();
        wxGraphicsContext* gctx=wxGraphicsContext::Create(dc);
        if (!gctx) return;
        gctx->SetAntialiasMode(wxANTIALIAS_DEFAULT);
        gctx->SetBrush(gctx->CreateLinearGradientBrush(0,0,0,panelH,wxColour(12,18,40),wxColour(22,35,70)));
        gctx->SetPen(*wxTRANSPARENT_PEN);
        gctx->DrawRectangle(0,0,panelW,panelH);
        gctx->SetFont(wxFont(14,wxFONTFAMILY_DEFAULT,wxFONTSTYLE_NORMAL,wxFONTWEIGHT_BOLD),*wxWHITE);
        gctx->DrawText(ru("Ходов: ")+to_string(moves), gx, 14);
        // Название уровня — по центру
        { gctx->SetFont(wxFont(14,wxFONTFAMILY_DEFAULT,wxFONTSTYLE_NORMAL,wxFONTWEIGHT_BOLD),wxColour(100,170,255));
          double tw,th; gctx->GetTextExtent(DIFFS[diffIndex].name,&tw,&th);
          gctx->DrawText(DIFFS[diffIndex].name,(panelW-tw)/2,14); }
        if (bestMoves>0) {
            gctx->SetFont(wxFont(14,wxFONTFAMILY_DEFAULT,wxFONTSTYLE_NORMAL,wxFONTWEIGHT_BOLD),wxColour(255,215,0));
            gctx->DrawText(ru("Рекорд: ")+to_string(bestMoves), panelW-gx-160, 14);
        }
        for (int i=0;i<total();i++) drawCard(gctx,i);
        delete gctx;
    }

    // Обработка клика мыши
    void onClick(wxMouseEvent& e) {
        if (waiting||animTimer.IsRunning()) return;
        computeLayout();
        int col=(e.GetX()-gx)/cellW, row=(e.GetY()-gy)/cellH;
        if (col<0||col>=COLS||row<0||row>=ROWS||e.GetX()<gx||e.GetY()<gy) return;
        int idx=row*COLS+col;
        if (cards[idx].matched||cards[idx].flipped||second>=0) return;
        cards[idx].flipped=true;
        if (!animTimer.IsRunning()) animTimer.Start(16);
        if (first<0) { first=idx; return; }
        second=idx; moves++;
        if (cards[first].value==cards[second].value) {
            cards[first].matched=cards[second].matched=true;
            matchedPairs++;
            first=second=-1;
            panel->Refresh();
            if (matchedPairs==total()/2) {
                bool newBest=(bestMoves==0||moves<bestMoves);
                string altScoreKey="Memory_"+to_string(COLS)+"x"+to_string(ROWS);
                    if (newBest) { bestMoves=moves; SaveHighScore(altScoreKey,moves); }
                wxString msg=ru("Все пары найдены!\nХодов: ")+to_string(moves);
                if (newBest) msg+=ru("\n\n*** Новый рекорд! ***");
                else msg+=ru("\nРекорд: ")+to_string(bestMoves)+ru(" ходов");
                msg+=ru("\n\nСыграть ещё раз?");
                int ans=wxMessageBox(msg,ru("Победа"),wxYES_NO|wxICON_INFORMATION,this);
                if (ans==wxYES){shuffle();panel->Refresh();}
                else goBack();
            }
        } else { waiting=true; flipTimer.Start(900,wxTIMER_ONE_SHOT); }
        panel->Refresh();
    }

    void updateDiffButtons() {
        for (int d = 0; d < 3; d++)
            diffBtns[d]->SetLabel(d == diffIndex
                ? "[" + DIFFS[d].name + "]"
                : DIFFS[d].name);
    }
    void onDiff(int d) {
        if (diffIndex == d) return;
        diffIndex = d;
        updateDiffButtons();
        shuffle(); panel->Refresh();
    }

    // Переход к меню игр
    void goBack();
    void onBack(wxCommandEvent&){goBack();}
    // Перезапуск игры
    void onRestart(wxCommandEvent&){shuffle();panel->Refresh();}

public:
    // Конструктор класса gamememory с инициализатором
    gamememory():wxFrame(nullptr,wxID_ANY,ru("Память"),wxDefaultPosition,wxSize(620,640)),
                 flipTimer(this),animTimer(this)
    {
        panel=new wxPanel(this);
        panel->SetBackgroundStyle(wxBG_STYLE_PAINT);
        // Установка цвета фона
        panel->SetBackgroundColour(wxColour(12,18,40));

        wxButton* b1=new wxButton(panel,wxID_ANY,ru("Меню"),wxPoint(10,10),wxSize(85,30));
        b1->Bind(wxEVT_BUTTON,&gamememory::onBack,this);
        wxButton* b2=new wxButton(panel,wxID_ANY,ru("Заново"),wxPoint(101,10),wxSize(85,30));
        b2->Bind(wxEVT_BUTTON,&gamememory::onRestart,this);

        // Кнопки уровней сложности
        wxString dnames[] = { ru("Лёгко"), ru("Норм."), ru("Сложно") };
        for (int d = 0; d < 3; d++) {
            diffBtns[d] = new wxButton(panel, wxID_ANY, dnames[d],
                wxPoint(310 + d*100, 10), wxSize(90, 30));
            diffBtns[d]->Bind(wxEVT_BUTTON, [this,d](wxCommandEvent&){ onDiff(d); });
        }

        wxBoxSizer* s=new wxBoxSizer(wxVERTICAL);
        s->Add(panel,1,wxEXPAND); SetSizer(s);
        panel->Bind(wxEVT_PAINT,    &gamememory::onPaint,    this);
        panel->Bind(wxEVT_LEFT_DOWN,&gamememory::onClick,    this);
        Bind(wxEVT_TIMER,&gamememory::onFlipTimer,this,flipTimer.GetId());
        Bind(wxEVT_TIMER,&gamememory::onAnimTimer,this,animTimer.GetId());
        shuffle();
        updateDiffButtons();
    }
};
gamememory::DiffConfig gamememory::DIFFS[3] = {
    {4, 3, wxString(L"Лёгко")},
    {4, 4, wxString(L"Нормально")},
    {6, 4, wxString(L"Сложно")},
};

// Класс gameLabirint наследует StubGame
class gameLabirint  : public StubGame { public: gameLabirint()  : StubGame("Labirint")    {} };
// Класс gameTag наследует StubGame
class gameTag       : public StubGame { public: gameTag()       : StubGame("Tag")         {} };
// TETRIS
// Класс gameBreakFour наследует wxFrame
class gameBreakFour : public wxFrame {
    static const int BW = 10, BH = 20; // ширина и высота поля
    int cs=28, gridBaseX=25, gridBaseY=50; // динамический размер клетки и позиция поля

    int  board[BH][BW] = {};  // 0=пусто, 1-7=цвет фигуры
    int  pType=0, pRot=0, pRow=0, pCol=0; // текущая фигура
    int  nextType = 0;
    int  score=0, level=1, lines=0;
    bool gameOver = false;
    wxPanel* panel;
    wxTimer  fallTimer;

    // Вычисление позиции и размера поля по размеру окна
    void computeLayout() {
        wxSize panelSz=panel->GetSize();
        int panelW=panelSz.GetWidth(), panelH=panelSz.GetHeight();
        cs=min((panelW-190)/BW,(panelH-65)/BH);
        if(cs<10)cs=10;
        gridBaseX=max(10,(panelW-190-cs*BW)/2);
        gridBaseY=max(50,(panelH-cs*BH)/2);
    }

    // Фигуры: [тип][поворот][ячейка][row,col]
    static const int SHAPES[7][4][4][2];

    static wxColour pieceCol(int t) {
        static const wxColour C[]={
            wxColour(0,0,0),
            wxColour(0,220,220),   // I cyan
            wxColour(220,220,0),   // O yellow
            wxColour(160,0,220),   // T purple
            wxColour(0,200,0),     // S green
            wxColour(220,0,0),     // Z red
            wxColour(0,80,220),    // J blue
            wxColour(220,140,0),   // L orange
        };
        return C[t];
    }
//динамический массив сосиоящий мз пар называемые cells и принимающие параметры типа рахмера столбцев и ячеек
    vector<pair<int,int>> cells(int t,int r,int row,int col) {
        //создает пустой массив
        vector<pair<int,int>> v;
        for(int i=0;i<4;i++)
        //добавляем к пустому массиву значение ячеек и столбцев занимаемая ячейкой
            v.push_back({row+SHAPES[t][r][i][0], col+SHAPES[t][r][i][1]});
        return v;
    }
 // функция можно ли поставить фигуру
    bool valid(int t,int r,int row,int col) {
        auto v=cells(t,r,row,col);
        for(int i=0;i<(int)v.size();i++) {
            int pieceR=v[i].first, pieceC=v[i].second;
            if(pieceR<0||pieceR>=BH||pieceC<0||pieceC>=BW||board[pieceR][pieceC]) return false;
        }
        return true;
    }
//зафиксироывние
    // Фиксация фигуры на поле
    void lock() {
        { auto lk=cells(pType,pRot,pRow,pCol);
          for(int i=0;i<(int)lk.size();i++) board[lk[i].first][lk[i].second]=pType+1; }
        clearLines(); spawn();
    }
//очиска линий
    // Удаление заполненных линий
    void clearLines() {
        int cleared=0;
        for(int r=BH-1;r>=0;) {
            bool full=true;
            for(int c=0;c<BW;c++) if(!board[r][c]){full=false;break;} //если одна клетка в линий не заполнена то false
            if(full) { //если заполнена
                for(int rr=r ;rr>0;rr--) for(int c=0;c<BW;c++) board[rr][c]=board[rr-1][c];
                for(int c=0;c<BW;c++) board[0][c]=0;
                cleared++;
            } else r--;
        }
        //дублтрует и спускает линий
        if(cleared) {
            static const int pts[]={0,100,300,500,800};
            score+=pts[min(cleared,4)]*level;
            lines+=cleared; level=lines/10+1;
            fallTimer.Start(max(80,500-(level-1)*40));
        }
    }
//спавн
    // Создание новой фигуры
    void spawn() {
        pType=nextType; pRot=0; pRow=0; pCol=BW/2-2; //новая фигура всегда появляеться по середине
        nextType=rand()%7; //рандомайзер случайного типа от 1 до 7 
        if(!valid(pType,pRot,pRow,pCol)) { //если не помецается
            gameOver=true; fallTimer.Stop(); panel->Refresh();
            wxString msg=ru("Игра окончена! Счёт: ")+to_string(score)+ru("\n\nЗаново?");
            if(wxMessageBox(msg,ru("Тетрис"),wxYES_NO|wxICON_INFORMATION,this)==wxYES)
                startGame();
        }
    }
//начало игры
    // Запуск игры
    void startGame() {
       for(int r=0; r<BH; r++)
       for(int c=0; c<BW; c++)
        board[r][c] = 0;
        score=0;level=1;lines=0;gameOver=false;
        nextType=rand()%7; 
        spawn();
        fallTimer.Start(500); panel->Refresh();
    }

    //рисование клетки

    // Рисование ячейки
    void drawCell(wxGraphicsContext* gctx,int r,int c,int ci,int boardX,int boardY) {
        //кординаты
        int x=boardX+c*cs, y=boardY+r*cs;
        if(!ci){ //на клетке нет фигуры
            //фон
            gctx->SetBrush(wxBrush(wxColour(18,24,48)));
            //обводка
            gctx->SetPen(gctx->CreatePen(wxGraphicsPenInfo(wxColour(28,36,70),1)));
            //квадратики
            gctx->DrawRectangle(x,y,cs,cs); return;
        }
        wxColour cellColor=pieceCol(ci); //определяем фигуру
        gctx->SetBrush(gctx->CreateLinearGradientBrush(x,y,x,y+cs, //градиент
            cellColor.ChangeLightness(130),cellColor.ChangeLightness(80)));
        //обводка
        gctx->SetPen(gctx->CreatePen(wxGraphicsPenInfo(cellColor.ChangeLightness(55),1)));
        gctx->DrawRoundedRectangle(x+1,y+1,cs-2,cs-2,3);
        //блики
        gctx->SetBrush(gctx->CreateLinearGradientBrush(x,y,x,y+cs/3,
            wxColour(255,255,255,70),wxColour(255,255,255,0)));
        gctx->SetPen(*wxTRANSPARENT_PEN);
        gctx->DrawRoundedRectangle(x+2,y+2,cs-4,cs/3,3);//скругление
    }

    // Отрисовка графики на экране
    void onPaint(wxPaintEvent&) {
        wxAutoBufferedPaintDC dc(panel);
        wxSize panelSz=panel->GetSize();
        int panelW=panelSz.GetWidth(), panelH =panelSz.GetHeight();
        wxGraphicsContext* gctx=wxGraphicsContext::Create(dc);
        if(!gctx) return;
        gctx->SetAntialiasMode(wxANTIALIAS_DEFAULT);

        gctx->SetBrush(gctx->CreateLinearGradientBrush(0,0,0,panelH,wxColour(12,18,40),wxColour(18,28,60)));
        gctx->SetPen(*wxTRANSPARENT_PEN);
        gctx->DrawRectangle(0,0,panelW,panelH);

        // Вычисляем позицию и размер поля по текущему размеру окна
        computeLayout();
        int boardX=gridBaseX, boardY=gridBaseY;

        gctx->SetBrush(wxBrush(wxColour(15,20,42)));
        gctx->SetPen(gctx->CreatePen(wxGraphicsPenInfo(wxColour(50,70,130),2)));
        gctx->DrawRoundedRectangle(boardX-3,boardY-3,BW*cs+6,BH*cs+6,6);

        for(int r=0;r<BH;r++) for(int c=0;c<BW;c++) drawCell(gctx,r,c,board[r][c],boardX,boardY);

        int ghostR=pRow;
        while(valid(pType,pRot,ghostR+1,pCol)) ghostR++;
        if(ghostR!=pRow) {
            wxColour shadowColor=pieceCol(pType+1);
            gctx->SetBrush(wxBrush(wxColour(shadowColor.Red(),shadowColor.Green(),shadowColor.Blue(),35)));
            gctx->SetPen(gctx->CreatePen(wxGraphicsPenInfo(wxColour(shadowColor.Red(),shadowColor.Green(),shadowColor.Blue(),70),1)));
            { auto ghost=cells(pType,pRot,ghostR,pCol);
            for(int i=0;i<(int)ghost.size();i++) {
                int r=ghost[i].first, c=ghost[i].second;
                if(r>=0) gctx->DrawRoundedRectangle(boardX+c*cs+1,boardY+r*cs+1,cs-2,cs-2,3);
            }}
        }

        { auto cur=cells(pType,pRot,pRow,pCol);
            for(int i=0;i<(int)cur.size();i++) {
                int r=cur[i].first, c=cur[i].second;
                if(r>=0) drawCell(gctx,r,c,pType+1,boardX,boardY);
            }}

        int infoX=boardX+BW*cs+18;
        auto drawLabel=[&](const wxString& s,int y){
            gctx->SetFont(wxFont(12,wxFONTFAMILY_DEFAULT,wxFONTSTYLE_NORMAL,wxFONTWEIGHT_BOLD),wxColour(130,160,210));
            gctx->DrawText(s,infoX,boardY+y);
        };
        auto drawValue=[&](const wxString& s,int y){
            gctx->SetFont(wxFont(20,wxFONTFAMILY_DEFAULT,wxFONTSTYLE_NORMAL,wxFONTWEIGHT_BOLD),*wxWHITE);
            gctx->DrawText(s,infoX,boardY+y+16);
        };
        drawLabel(ru("Счёт"),0);   drawValue(to_string(score),0);
        drawLabel(ru("Уровень"),65); drawValue(to_string(level),65);
        drawLabel(ru("Линии"),130); drawValue(to_string(lines),130);

        drawLabel(ru("Следующая"),200);
        int nextPieceX=infoX, nextPieceY=boardY+222;
        gctx->SetBrush(wxBrush(wxColour(15,20,42)));
        gctx->SetPen(gctx->CreatePen(wxGraphicsPenInfo(wxColour(50,70,130),1)));
        gctx->DrawRoundedRectangle(nextPieceX-2,nextPieceY-2,4*cs+4,4*cs+4,5);
        { auto nxt=cells(nextType,0,0,0);
            for(int i=0;i<(int)nxt.size();i++)
                drawCell(gctx,nxt[i].first,nxt[i].second,nextType+1,nextPieceX,nextPieceY); }

        gctx->SetFont(wxFont(10,wxFONTFAMILY_DEFAULT,wxFONTSTYLE_ITALIC,wxFONTWEIGHT_NORMAL),wxColour(90,110,160));
        gctx->DrawText(ru("←→ / A D — движение"), infoX,boardY+390);
        gctx->DrawText(ru("↓ / S — ускорить"),    infoX,boardY+406);
        gctx->DrawText(ru("↑ / W — поворот"),     infoX,boardY+422);
        gctx->DrawText(ru("Пробел — сброс"),       infoX,boardY+438);

        delete gctx;
    }

    // Обработчик таймера — автоматический шаг
    void onTimer(wxTimerEvent&) {
        if(valid(pType,pRot,pRow+1,pCol)) pRow++;
        else lock();
        panel->Refresh();
    }

    // Обработка нажатия клавиши
    void onKeyDown(wxKeyEvent& e) {
        if(gameOver){e.Skip();return;}
        int key = e.GetKeyCode();
        // Нормализуем WASD → стрелки
        if (key=='A'||key=='a') key=WXK_LEFT;
        else if(key=='D'||key=='d') key=WXK_RIGHT;
        else if(key=='S'||key=='s') key=WXK_DOWN;
        else if(key=='W'||key=='w') key=WXK_UP;
        switch(key){
        case WXK_LEFT:  if(valid(pType,pRot,pRow,pCol-1)) pCol--; break;
        case WXK_RIGHT: if(valid(pType,pRot,pRow,pCol+1)) pCol++; break;
        case WXK_DOWN:
            if(valid(pType,pRot,pRow+1,pCol)) pRow++;
            else lock(); break;
        case WXK_UP: {
            int newRot=(pRot+1)%4;
            if(valid(pType,newRot,pRow,pCol))   {pRot=newRot;}
            else if(valid(pType,newRot,pRow,pCol+1)){pRot=newRot;pCol++;}
            else if(valid(pType,newRot,pRow,pCol-1)){pRot=newRot;pCol--;}
            break;}
        case WXK_SPACE:
            while(valid(pType,pRot,pRow+1,pCol)) pRow++;
            lock(); break;
        default: e.Skip(); return;
        }
        panel->Refresh();
    }

    void onBack(wxCommandEvent&);
    // Перезапуск игры
    void onRestart(wxCommandEvent&){startGame();}

public:
    // Конструктор класса gameBreakFour с инициализатором
    gameBreakFour():wxFrame(nullptr,wxID_ANY,ru("Тетрис"),
                             wxDefaultPosition,wxSize(530,640)),
                    fallTimer(this)
    {
        panel=new wxPanel(this);
        panel->SetBackgroundStyle(wxBG_STYLE_PAINT);
        // Установка цвета фона
        panel->SetBackgroundColour(wxColour(12,18,40));

        wxButton* backBtn=new wxButton(panel,wxID_ANY,ru("Меню"),wxPoint(10,10),wxSize(90,30));
        backBtn->Bind(wxEVT_BUTTON,&gameBreakFour::onBack,this);
        wxButton* restBtn=new wxButton(panel,wxID_ANY,ru("Заново"),wxPoint(108,10),wxSize(90,30));
        restBtn->Bind(wxEVT_BUTTON,&gameBreakFour::onRestart,this);

        wxBoxSizer* sizer=new wxBoxSizer(wxVERTICAL);
        sizer->Add(panel,1,wxEXPAND);
        SetSizer(sizer);

        panel->Bind(wxEVT_PAINT,&gameBreakFour::onPaint,this);
        Bind(wxEVT_SIZE,[this](wxSizeEvent& e){ panel->Refresh(); e.Skip(); });
        Bind(wxEVT_TIMER,&gameBreakFour::onTimer,this,fallTimer.GetId());
        Bind(wxEVT_CHAR_HOOK,&gameBreakFour::onKeyDown,this);

        nextType=rand()%7;
        startGame();
        CallAfter([this](){panel->SetFocus();});
    }
};
const int gameBreakFour::SHAPES[7][4][4][2] = {
    // I
    {{{1,0},{1,1},{1,2},{1,3}},{{0,2},{1,2},{2,2},{3,2}},{{2,0},{2,1},{2,2},{2,3}},{{0,1},{1,1},{2,1},{3,1}}},
    // O
    {{{0,1},{0,2},{1,1},{1,2}},{{0,1},{0,2},{1,1},{1,2}},{{0,1},{0,2},{1,1},{1,2}},{{0,1},{0,2},{1,1},{1,2}}},
    // T
    {{{0,1},{1,0},{1,1},{1,2}},{{0,1},{1,1},{1,2},{2,1}},{{1,0},{1,1},{1,2},{2,1}},{{0,1},{1,0},{1,1},{2,1}}},
    // S
    {{{0,1},{0,2},{1,0},{1,1}},{{0,1},{1,1},{1,2},{2,2}},{{1,1},{1,2},{2,0},{2,1}},{{0,0},{1,0},{1,1},{2,1}}},
    // Z
    {{{0,0},{0,1},{1,1},{1,2}},{{0,2},{1,1},{1,2},{2,1}},{{1,0},{1,1},{2,1},{2,2}},{{0,1},{1,0},{1,1},{2,0}}},
    // J
    {{{0,0},{1,0},{1,1},{1,2}},{{0,1},{0,2},{1,1},{2,1}},{{1,0},{1,1},{1,2},{2,2}},{{0,1},{1,1},{2,0},{2,1}}},
    // L
    {{{0,2},{1,0},{1,1},{1,2}},{{0,1},{1,1},{2,1},{2,2}},{{1,0},{1,1},{1,2},{2,0}},{{0,0},{0,1},{1,1},{2,1}}},
};

// ─── Крестики-нолики ──────────────────────────────────────────────────────────
// Класс gameTicTac наследует wxFrame
class gameTicTac : public wxFrame {
    int  board[3][3] = {};  // 0=пусто 1=X 2=O
    int  current = 1;       // чей ход
    bool gameOver = false;
    int  winLine[3][2] = {}; // победная линия для анимации
    bool hasWinLine = false;
    int  hoverCell = -1;    // -1 или r*3+c
    wxPanel* panel;

    int cellSize = 160, gridX = 0, gridY = 0;

    // Вычисление позиций элементов интерфейса
    void computeLayout() {
        wxSize panelSz = panel->GetSize();
        int panelW = panelSz.GetWidth(), panelH = panelSz.GetHeight();
        cellSize = min((panelW-60)/3, (panelH-100)/3);
        gridX = (panelW - cellSize*3)/2;
        gridY = 55 + (panelH - 55 - cellSize*3)/2;
    }

    // Проверка победы, заполняет winLine если есть
    // Проверка победы
    int checkWin() {
        auto eq = [&](int r0,int c0,int r1,int c1,int r2,int c2){
            int v=board[r0][c0];
            if(v&&v==board[r1][c1]&&v==board[r2][c2]){
                winLine[0][0]=r0;winLine[0][1]=c0;
                winLine[1][0]=r1;winLine[1][1]=c1;
                winLine[2][0]=r2;winLine[2][1]=c2;
                hasWinLine=true; return v;
            } return 0;
        };
        int w=0;
        for(int i=0;i<3&&!w;i++) w=eq(i,0,i,1,i,2); // строки
        for(int i=0;i<3&&!w;i++) w=eq(0,i,1,i,2,i); // столбцы
        if(!w) w=eq(0,0,1,1,2,2);
        if(!w) w=eq(0,2,1,1,2,0);
        return w;
    }

    // Проверка ничьей
    bool isDraw() {
        for(int r=0;r<3;r++) for(int c=0;c<3;c++) if(!board[r][c]) return false;
        return true;
    }

    void reset() {
        for(int r=0;r<3;r++) for(int c=0;c<3;c++) board[r][c]=0;
        current=1; gameOver=false; hasWinLine=false; hoverCell=-1;
        panel->Refresh();
    }

    // ── Отрисовка ─────────────────────────────────────────────────────────────
    void drawX(wxGraphicsContext* gctx, double centX, double centY, double r, double alpha=1.0) {
        wxColour cellColor(220,80,80, (unsigned char)(255*alpha));
        gctx->SetPen(gctx->CreatePen(wxGraphicsPenInfo(cellColor, r*0.18).Cap(wxCAP_ROUND)));
        double d = r*0.6;
        gctx->StrokeLine(centX-d,centY-d,centX+d,centY+d);
        gctx->StrokeLine(centX+d,centY-d,centX-d,centY+d);
    }

    void drawO(wxGraphicsContext* gctx, double centX, double centY, double r, double alpha=1.0) {
        wxColour cellColor(80,160,220, (unsigned char)(255*alpha));
        gctx->SetBrush(*wxTRANSPARENT_BRUSH);
        gctx->SetPen(gctx->CreatePen(wxGraphicsPenInfo(cellColor, r*0.16).Cap(wxCAP_ROUND)));
        double d = r*0.58;
        gctx->DrawEllipse(centX-d, centY-d, d*2, d*2);
    }

    // Отрисовка графики на экране
    void onPaint(wxPaintEvent&) {
        wxAutoBufferedPaintDC dc(panel);
        wxSize panelSz = panel->GetSize();
        int panelW=panelSz.GetWidth(), panelH =panelSz.GetHeight();
        computeLayout();

        wxGraphicsContext* gctx = wxGraphicsContext::Create(dc);
        if (!gctx) return;
        gctx->SetAntialiasMode(wxANTIALIAS_DEFAULT);

        // Фон
        gctx->SetBrush(gctx->CreateLinearGradientBrush(0,0,0,panelH,
            wxColour(12,18,40), wxColour(22,35,70)));
        gctx->SetPen(*wxTRANSPARENT_PEN);
        gctx->DrawRectangle(0,0,panelW,panelH);

        // Индикатор хода
        if (!gameOver) {
            wxColour tc = current==1 ? wxColour(220,80,80) : wxColour(80,160,220);
            wxString who = current==1 ? ru("Ход: X (красный)") : ru("Ход: O (синий)");
            gctx->SetFont(wxFont(15,wxFONTFAMILY_DEFAULT,wxFONTSTYLE_NORMAL,wxFONTWEIGHT_BOLD), tc);
            gctx->DrawText(who, gridX, 14);
        }

        // Подсветка ячейки под курсором
        if (hoverCell>=0 && !gameOver) {
            int hr=hoverCell/3, hc=hoverCell%3;
            if (!board[hr][hc]) {
                gctx->SetBrush(wxBrush(wxColour(255,255,255,15)));
                gctx->SetPen(*wxTRANSPARENT_PEN);
                gctx->DrawRoundedRectangle(gridX+hc*cellSize+4, gridY+hr*cellSize+4,
                    cellSize-8, cellSize-8, 12);
                // Превью символа
                double centX=gridX+hc*cellSize+cellSize/2.0;
                double centY=gridY+hr*cellSize+cellSize/2.0;
                double r=cellSize*0.3;
                if (current==1) drawX(gctx,centX,centY,r,0.25);
                else            drawO(gctx,centX,centY,r,0.25);
            }
        }

        // Линии сетки
        gctx->SetPen(gctx->CreatePen(wxGraphicsPenInfo(wxColour(80,110,180), 3).Cap(wxCAP_ROUND)));
        for (int i=1;i<3;i++) {
            gctx->StrokeLine(gridX+i*cellSize, gridY+10, gridX+i*cellSize, gridY+3*cellSize-10);
            gctx->StrokeLine(gridX+10, gridY+i*cellSize, gridX+3*cellSize-10, gridY+i*cellSize);
        }

        // Символы
        for (int r=0;r<3;r++) for (int c=0;c<3;c++) {
            if (!board[r][c]) continue;
            double centX=gridX+c*cellSize+cellSize/2.0;
            double centY=gridY+r*cellSize+cellSize/2.0;
            double rad=cellSize*0.32;
            if (board[r][c]==1) drawX(gctx,centX,centY,rad);
            else                drawO(gctx,centX,centY,rad);
        }

        // Линия победы
        if (hasWinLine) {
            double x0=gridX+winLine[0][1]*cellSize+cellSize/2.0;
            double y0=gridY+winLine[0][0]*cellSize+cellSize/2.0;
            double x1=gridX+winLine[2][1]*cellSize+cellSize/2.0;
            double y1=gridY+winLine[2][0]*cellSize+cellSize/2.0;
            wxColour wc = board[winLine[0][0]][winLine[0][1]]==1
                ? wxColour(255,120,120) : wxColour(120,200,255);
            gctx->SetPen(gctx->CreatePen(wxGraphicsPenInfo(wc,8).Cap(wxCAP_ROUND)));
            gctx->StrokeLine(x0,y0,x1,y1);
        }

        delete gctx;
    }

    // Обработка движения мыши
    void onMouseMove(wxMouseEvent& e) {
        computeLayout();
        int mouseX=e.GetX(), mouseY=e.GetY();
        int c=(mouseX-gridX)/cellSize, r=(mouseY-gridY)/cellSize;
        int newHover=(r>=0&&r<3&&c>=0&&c<3&&mouseX>=gridX&&mouseY>=gridY)?r*3+c:-1;
        if (newHover!=hoverCell){hoverCell=newHover;panel->Refresh();}
    }

    // Обработка клика мыши
    void onMouseClick(wxMouseEvent& e) {
        if (gameOver) return;
        computeLayout();
        int mouseX=e.GetX(), mouseY=e.GetY();
        int c=(mouseX-gridX)/cellSize, r=(mouseY-gridY)/cellSize;
        if (r<0||r>=3||c<0||c>=3||mouseX<gridX||mouseY<gridY) return;
        if (board[r][c]) return;

        board[r][c]=current;
        panel->Refresh();

        int winner=checkWin();
        if (winner) {
            gameOver=true;
            panel->Refresh();
            wxString who=winner==1?ru("Крестики (X)"):ru("Нолики (O)");
            int ans=wxMessageBox(who+ru(" победили!\n\nСыграть ещё раз?"),
                ru("Победа"),wxYES_NO|wxICON_INFORMATION,this);
            if(ans==wxYES) reset();
            return;
        }
        if (isDraw()) {
            gameOver=true;
            int ans=wxMessageBox(ru("Ничья!\n\nСыграть ещё раз?"),
                ru("Ничья"),wxYES_NO|wxICON_INFORMATION,this);
            if(ans==wxYES) reset();
            return;
        }
        current=(current==1)?2:1;
    }

    void onBack(wxCommandEvent&);
    // Перезапуск игры
    void onRestart(wxCommandEvent&){reset();}

public:
    // Конструктор класса gameTicTac с инициализатором
    gameTicTac():wxFrame(nullptr,wxID_ANY,ru("Крестики-нолики"),
                          wxDefaultPosition,wxSize(560,620))
    {
        panel=new wxPanel(this);
        panel->SetBackgroundStyle(wxBG_STYLE_PAINT);
        // Установка цвета фона
        panel->SetBackgroundColour(wxColour(12,18,40));

        wxButton* backBtn=new wxButton(panel,wxID_ANY,ru("Меню"),wxPoint(10,10),wxSize(90,30));
        backBtn->Bind(wxEVT_BUTTON,&gameTicTac::onBack,this);

        wxButton* restBtn=new wxButton(panel,wxID_ANY,ru("Заново"),wxPoint(108,10),wxSize(90,30));
        restBtn->Bind(wxEVT_BUTTON,&gameTicTac::onRestart,this);

        wxBoxSizer* sizer=new wxBoxSizer(wxVERTICAL);
        sizer->Add(panel,1,wxEXPAND);
        SetSizer(sizer);

        panel->Bind(wxEVT_PAINT,    &gameTicTac::onPaint,      this);
        panel->Bind(wxEVT_LEFT_DOWN,&gameTicTac::onMouseClick, this);
        panel->Bind(wxEVT_MOTION,   &gameTicTac::onMouseMove,  this);
    }
};
// ─── Настройки судоку ────────────────────────────────────────────────────────
// Класс SudokuSettingsDialog наследует wxDialog
class SudokuSettingsDialog : public wxDialog {
    int& removals;
    int& maxLives;
    wxRadioBox* diffRadio;
    wxRadioBox* livesRadio;

    void OnApply(wxCommandEvent&) {
        switch (diffRadio->GetSelection()) {
            case 0: removals = 30; break;
            case 1: removals = 40; break;
            case 2: removals = 55; break;
        }
        switch (livesRadio->GetSelection()) {
            case 0: maxLives =  3; break;
            case 1: maxLives =  5; break;
            case 2: maxLives = -1; break; // безлимит
        }
        EndModal(wxID_OK);
    }

public:
    SudokuSettingsDialog(wxWindow* parent, int& rem, int& ml)
        : wxDialog(parent, wxID_ANY, ru("Настройки"),
                   wxDefaultPosition, wxSize(300, 300),
                   wxDEFAULT_DIALOG_STYLE),
          removals(rem), maxLives(ml)
    {
        SetBackgroundColour(wxColour(30,30,30));
        wxBoxSizer* main = new wxBoxSizer(wxVERTICAL);

        wxString diffs[] = { ru("Легко (30)"), ru("Нормально (40)"), ru("Сложно (55)") };
        diffRadio = new wxRadioBox(this, wxID_ANY, ru("Сложность"),
            wxDefaultPosition, wxDefaultSize, 3, diffs, 1, wxRA_SPECIFY_COLS);
        diffRadio->SetSelection(rem <= 30 ? 0 : rem <= 40 ? 1 : 2);
        // Установка цвета фона
        diffRadio->SetBackgroundColour(wxColour(30,30,30));
        // Установка цвета текста
        diffRadio->SetForegroundColour(*wxWHITE);
        main->Add(diffRadio, 0, wxEXPAND | wxALL, 15);

        wxString livesOpts[] = { ru("3 жизни"), ru("5 жизней"), ru("Безлимит") };
        livesRadio = new wxRadioBox(this, wxID_ANY, ru("Жизни"),
            wxDefaultPosition, wxDefaultSize, 3, livesOpts, 1, wxRA_SPECIFY_COLS);
        livesRadio->SetSelection(ml == 3 ? 0 : ml == 5 ? 1 : 2);
        // Установка цвета фона
        livesRadio->SetBackgroundColour(wxColour(30,30,30));
        // Установка цвета текста
        livesRadio->SetForegroundColour(*wxWHITE);
        main->Add(livesRadio, 0, wxEXPAND | wxALL, 15);

        wxButton* applyBtn = new wxButton(this, wxID_ANY, ru("Применить"),
            wxDefaultPosition, wxSize(120,35));
        // Установка цвета фона
        applyBtn->SetBackgroundColour(wxColour(0,150,0));
        // Установка цвета текста
        applyBtn->SetForegroundColour(*wxWHITE);
        applyBtn->Bind(wxEVT_BUTTON, &SudokuSettingsDialog::OnApply, this);
        main->Add(applyBtn, 0, wxALIGN_CENTER | wxALL, 15);

        SetSizer(main);
    }
};

// ─── Судоку ───────────────────────────────────────────────────────────────────
// Класс gameSudocu наследует wxFrame
class gameSudocu : public wxFrame {
    static const int N = 9;

    int  solution[N][N];
    int  board[N][N];
    bool given[N][N];

    int selRow = -1, selCol = -1;
    int highlightNum = 0; // цифра для подсветки совпадающих ячеек (0 = нет)
    int gridX = 0, gridY = 0, cellSize = 50;
    int lives    = 3;
    int maxLives = 3;   // -1 = безлимит
    int removals = 40;
    wxPanel* panel;

    // ── Генерация ─────────────────────────────────────────────────────────────
    // Проверка допустимости хода
    bool isValid(int g[N][N], int r, int c, int num) {
        for (int i = 0; i < N; i++)
            if (g[r][i] == num || g[i][c] == num) return false;
        int boxRow = (r/3)*3, boxCol = (c/3)*3;
        for (int i = 0; i < 3; i++)
            for (int j = 0; j < 3; j++)
                if (g[boxRow+i][boxCol+j] == num) return false;
        return true;
    }

    // Решение головоломки рекурсией
    bool solve(int g[N][N]) {
        for (int r = 0; r < N; r++) {
            for (int c = 0; c < N; c++) {
                if (g[r][c] != 0) continue;
                int nums[9] = {1,2,3,4,5,6,7,8,9};
                for (int i = 8; i > 0; i--) swap(nums[i], nums[rand()%(i+1)]);
                for (int n : nums) {
                    if (isValid(g, r, c, n)) {
                        g[r][c] = n;
                        if (solve(g)) return true;
                        g[r][c] = 0;
                    }
                }
                return false;
            }
        }
        return true;
    }

    // Генерация новой головоломки
    void generate() {
        for(int r=0;r<N;r++) for(int c=0;c<N;c++) solution[r][c]=0;
        solve(solution);
        for(int r=0;r<N;r++) for(int c=0;c<N;c++) board[r][c]=solution[r][c];
        for(int r=0;r<N;r++) for(int c=0;c<N;c++) given[r][c]=true;

        vector<pair<int,int>> cells;
        for (int r = 0; r < N; r++)
            for (int c = 0; c < N; c++)
                cells.push_back({r, c});
        for (int i = (int)cells.size()-1; i > 0; i--)
            swap(cells[i], cells[rand()%(i+1)]);
        for (int k = 0; k < removals; k++) {
            auto [r,c] = cells[k];
            board[r][c] = 0;
            given[r][c] = false;
        }
        lives = (maxLives == -1) ? 999 : maxLives;
        selRow = selCol = -1;
        highlightNum = 0;
    }

    // ── Проверка ─────────────────────────────────────────────────────────────
    bool isCellError(int r, int c) {
        return board[r][c] != 0 && board[r][c] != solution[r][c];
    }

    bool isComplete() {
        for (int r = 0; r < N; r++)
            for (int c = 0; c < N; c++)
                if (board[r][c] != solution[r][c]) return false;
        return true;
    }

    // ── Раскладка ────────────────────────────────────────────────────────────
    // Вычисление позиций элементов интерфейса
    void computeLayout() {
        wxSize panelSz = panel->GetSize();
        int panelW = panelSz.GetWidth(), panelH = panelSz.GetHeight();
        if (panelW < 100 || panelH < 100) return;
        cellSize = min((panelW - 40) / N, (panelH - 90) / N);
        gridX = (panelW - cellSize * N) / 2;
        gridY = 55 + (panelH - 55 - cellSize * N) / 2;
    }

    // ── Отрисовка ────────────────────────────────────────────────────────────
    // Отрисовка графики на экране
    void onPaint(wxPaintEvent&) {
        wxAutoBufferedPaintDC dc(panel);
        wxSize panelSz = panel->GetSize();
        int panelW = panelSz.GetWidth(), panelH = panelSz.GetHeight();
        computeLayout();

        wxGraphicsContext* gctx = wxGraphicsContext::Create(dc);
        if (!gctx) return;
        gctx->SetAntialiasMode(wxANTIALIAS_DEFAULT);

        // Фон
        gctx->SetBrush(gctx->CreateLinearGradientBrush(0,0,0,panelH,
            wxColour(12,18,40), wxColour(22,35,70)));
        gctx->SetPen(*wxTRANSPARENT_PEN);
        gctx->DrawRectangle(0,0,panelW,panelH);

        // Подсветка
        if (selRow >= 0) {
            gctx->SetBrush(wxBrush(wxColour(50,80,140,40)));
            gctx->SetPen(*wxTRANSPARENT_PEN);
            gctx->DrawRectangle(gridX, gridY+selRow*cellSize, N*cellSize, cellSize);
            gctx->DrawRectangle(gridX+selCol*cellSize, gridY, cellSize, N*cellSize);
            int boxRow=(selRow/3)*3, boxCol=(selCol/3)*3;
            gctx->SetBrush(wxBrush(wxColour(60,90,160,35)));
            gctx->DrawRectangle(gridX+boxCol*cellSize, gridY+boxRow*cellSize, 3*cellSize, 3*cellSize);
            gctx->SetBrush(wxBrush(wxColour(90,130,220,90)));
            gctx->DrawRectangle(gridX+selCol*cellSize, gridY+selRow*cellSize, cellSize, cellSize);
        }

        // Подсветка одинаковых цифр
        if (highlightNum > 0) {
            gctx->SetPen(*wxTRANSPARENT_PEN);
            for (int r = 0; r < N; r++) {
                for (int c = 0; c < N; c++) {
                    if (board[r][c] != highlightNum) continue;
                    bool isSelected = (r == selRow && c == selCol);
                    // совпадающие ячейки — янтарный фон; выбранная уже подсвечена выше
                    if (!isSelected) {
                        gctx->SetBrush(wxBrush(wxColour(200,160,30,70)));
                        gctx->DrawRectangle(gridX+c*cellSize, gridY+r*cellSize, cellSize, cellSize);
                    }
                }
            }
        }

        // Числа
        int fs = max(12, cellSize * 45 / 100);
        for (int r = 0; r < N; r++) {
            for (int c = 0; c < N; c++) {
                if (board[r][c] == 0) continue;
                wxColour cellColor = given[r][c]      ? *wxWHITE
                             : isCellError(r,c) ? wxColour(255,80,80)
                             :                    wxColour(100,190,255);
                gctx->SetFont(wxFont(fs, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL,
                    given[r][c] ? wxFONTWEIGHT_BOLD : wxFONTWEIGHT_NORMAL), cellColor);
                wxString num = wxString::Format("%d", board[r][c]);
                double textWidth, textHeight;
                gctx->GetTextExtent(num, &textWidth, &textHeight);
                gctx->DrawText(num,
                    gridX+c*cellSize+(cellSize-textWidth)/2,
                    gridY+r*cellSize+(cellSize-textHeight)/2);
            }
        }

        // Сетка
        gctx->SetPen(gctx->CreatePen(wxGraphicsPenInfo(wxColour(70,95,150), 1)));
        for (int i = 1; i < N; i++) {
            if (i%3==0) continue;
            gctx->StrokeLine(gridX+i*cellSize, gridY, gridX+i*cellSize, gridY+N*cellSize);
            gctx->StrokeLine(gridX, gridY+i*cellSize, gridX+N*cellSize, gridY+i*cellSize);
        }
        gctx->SetPen(gctx->CreatePen(wxGraphicsPenInfo(wxColour(160,190,255), 2.5)));
        for (int i = 0; i <= N; i+=3) {
            gctx->StrokeLine(gridX+i*cellSize, gridY, gridX+i*cellSize, gridY+N*cellSize);
            gctx->StrokeLine(gridX, gridY+i*cellSize, gridX+N*cellSize, gridY+i*cellSize);
        }

        // Жизни (кружки справа сверху)
        gctx->SetFont(wxFont(13, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL,
                           wxFONTWEIGHT_BOLD), *wxWHITE);
        gctx->DrawText(ru("Жизни:"), panelW-200, 14);

        if (maxLives == -1) {
            gctx->DrawText("INF", panelW-115, 14);
        } else {
            for (int i = 0; i < maxLives; i++) {
                int lifeX = panelW - 110 + i * 22;
                wxColour lifeColor = (i < lives) ? wxColour(220,60,60) : wxColour(60,60,80);
                gctx->SetBrush(gctx->CreateRadialGradientBrush(lifeX-3,17, lifeX,20, 9,
                    lifeColor.ChangeLightness(140), lifeColor));
                gctx->SetPen(*wxTRANSPARENT_PEN);
                gctx->DrawEllipse(lifeX-9, 11, 18, 18);
            }
        }

        delete gctx;
    }

    // ── Управление ───────────────────────────────────────────────────────────
    // Обработка клика мыши
    void onMouseClick(wxMouseEvent& e) {
        computeLayout();
        int c = (e.GetX()-gridX)/cellSize;
        int r = (e.GetY()-gridY)/cellSize;
        if (r>=0 && r<N && c>=0 && c<N && e.GetX()>=gridX && e.GetY()>=gridY) {
            selRow=r; selCol=c;
            highlightNum = board[selRow][selCol]; // 0 если пустая
        } else {
            selRow=selCol=-1;
            highlightNum = 0;
        }
        panel->SetFocus();
        panel->Refresh();
    }

    // Обработка нажатия клавиши
    void onKeyDown(wxKeyEvent& e) {
        if (selRow < 0) { e.Skip(); return; }
        int key = e.GetKeyCode();

        if (key >= '1' && key <= '9') {
            int num = key - '0';
            highlightNum = num; // подсветить все совпадающие сразу при нажатии
            if (!given[selRow][selCol]) {
                board[selRow][selCol] = num;

                if (num != solution[selRow][selCol]) {
                    // Неверный ответ
                    if (maxLives != -1) {
                        lives--;
                        panel->Refresh();
                        if (lives <= 0) {
                            wxMessageBox(ru("Жизни закончились! Игра окончена."),
                                ru("Конец игры"), wxOK | wxICON_WARNING, this);
                            generate(); panel->Refresh(); return;
                        }
                    }
                }

                panel->Refresh();
                if (isComplete()) {
                    wxString msg = ru("Поздравляем! Судоку решено!");
                    if (maxLives != -1)
                        msg += wxString::Format(ru("\nОсталось жизней: %d"), lives);
                    wxMessageBox(msg, ru("Победа"), wxOK | wxICON_INFORMATION, this);
                    generate(); panel->Refresh();
                }
            }
        } else if (key == WXK_DELETE || key == WXK_BACK) {
            if (!given[selRow][selCol]) { board[selRow][selCol]=0; highlightNum=0; panel->Refresh(); }
        } else if (key == WXK_UP    && selRow>0) { selRow--; panel->Refresh(); }
        else if  (key == WXK_DOWN  && selRow<8) { selRow++; panel->Refresh(); }
        else if  (key == WXK_LEFT  && selCol>0) { selCol--; panel->Refresh(); }
        else if  (key == WXK_RIGHT && selCol<8) { selCol++; panel->Refresh(); }
        else e.Skip();
    }

    // Открытие настроек
    void onSettings(wxCommandEvent&) {
        SudokuSettingsDialog dlg(this, removals, maxLives);
        dlg.ShowModal();
        generate(); panel->Refresh();
    }

    void onBack(wxCommandEvent&);
    // Перезапуск игры
    void onRestart(wxCommandEvent&) { generate(); panel->Refresh(); }

public:
    // Конструктор класса gameSudocu с инициализатором
    gameSudocu() : wxFrame(nullptr, wxID_ANY, ru("Судоку"),
                            wxDefaultPosition, wxSize(620, 680))
    {
        panel = new wxPanel(this);
        panel->SetBackgroundStyle(wxBG_STYLE_PAINT);
        // Установка цвета фона
        panel->SetBackgroundColour(wxColour(12,18,40));

        wxButton* backBtn = new wxButton(panel, wxID_ANY, ru("Меню"),
            wxPoint(10,10), wxSize(90,30));
        backBtn->Bind(wxEVT_BUTTON, &gameSudocu::onBack, this);

        wxButton* newBtn = new wxButton(panel, wxID_ANY, ru("Новая игра"),
            wxPoint(108,10), wxSize(105,30));
        newBtn->Bind(wxEVT_BUTTON, &gameSudocu::onRestart, this);

        wxButton* settBtn = new wxButton(panel, wxID_ANY, ru("Настройки"),
            wxPoint(221,10), wxSize(105,30));
        settBtn->Bind(wxEVT_BUTTON, &gameSudocu::onSettings, this);

        wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
        sizer->Add(panel, 1, wxEXPAND);
        SetSizer(sizer);

        panel->Bind(wxEVT_PAINT,     &gameSudocu::onPaint,      this);
        panel->Bind(wxEVT_LEFT_DOWN, &gameSudocu::onMouseClick, this);
        Bind(wxEVT_CHAR_HOOK,        &gameSudocu::onKeyDown,     this);

        generate();
        CallAfter([this]() { panel->SetFocus(); });
    }
};
// ─── Колбочки (Water Sort) ───────────────────────────────────────────────────
// Класс gameColbs наследует wxFrame
class gameColbs : public wxFrame {
    static const int CAP = 4;

    struct DiffConfig { int colors, empty; wxString name; };
    static DiffConfig DIFFS[3];

    int diffIndex = 1; // 0=Легко 1=Нормально 2=Сложно

    wxPanel*      panel;
    wxStaticText* movesLabel;
    wxButton*     diffBtns[3];
    wxButton*     undoBtn;

    vector<vector<int>>              tubes;
    vector<vector<vector<int>>>      history; // стек для undo
    int sel   = 0;
    int moves = 0;

    // ── Анимация перелива ─────────────────────────────────────────────────────
    wxTimer animTimer;
    bool    animating  = false;
    int     animFrom   = -1, animTo = -1;
    int     animColor  = -1, animCount = 0;
    float   animT      = 0.f;

    int numColors() { return DIFFS[diffIndex].colors; }
    int numEmpty()  { return DIFFS[diffIndex].empty;  }
    int numTubes()  { return numColors() + numEmpty(); }

    static wxColour pal(int c) {
        static const wxColour P[] = {
            wxColour(220, 65,  65),   // красный
            wxColour(65,  130, 220),  // синий
            wxColour(65,  185, 90),   // зелёный
            wxColour(220, 190, 50),   // жёлтый
            wxColour(170, 65,  210),  // фиолетовый
            wxColour(220, 130, 50),   // оранжевый
            wxColour(50,  200, 200),  // бирюзовый
            wxColour(220, 80,  160),  // розовый
        };
        return P[c % 8];
    }

    wxRect tubeRect(int i) {
        int panelW     = panel->GetSize().GetWidth();
        int total = numTubes();
        int perRow = (total + 1) / 2;
        int row = i / perRow, col = i % perRow;
        int thisRowCount = (row == 0) ? perRow : (total - perRow);
        int textWidth = 56, cellH = 40, textHeight = CAP * cellH;
        int spacing = min((panelW - 40) / max(thisRowCount,1), 120);
        int startX = (panelW - thisRowCount * spacing) / 2;
        int centX = startX + col * spacing + spacing / 2;
        int byRow[] = { 280, 500 };
        return wxRect(centX - textWidth/2, byRow[row] - textHeight, textWidth, textHeight);
    }

    int hitTest(int mouseX, int mouseY) {
        if (animating) return -1;
        for (int i = 0; i < numTubes(); i++) {
            wxRect r = tubeRect(i);
            r.Inflate(12, 25);
            if (r.Contains(mouseX, mouseY)) return i;
        }
        return -1;
    }

    // Обновление счётчика ходов на экране
    void updateMovesLabel() {
        if (movesLabel)
            movesLabel->SetLabel(ru("Ходов: ") + to_string(moves));
    }

    void updateDiffButtons() {
        for (int d = 0; d < 3; d++)
            diffBtns[d]->SetLabel(d == diffIndex
                ? "[" + DIFFS[d].name + "]"
                : DIFFS[d].name);
    }

    void saveState() { history.push_back(tubes); }

    void undo() {
        if (history.empty() || animating) return;
        tubes = history.back();
        history.pop_back();
        if (moves > 0) moves--;
        updateMovesLabel();
        if (undoBtn) undoBtn->Enable(!history.empty());
        sel = -1;
        panel->Refresh();
    }

    // Инициализация начального состояния
    void init() {
        int nc = numColors();
        tubes.assign(numTubes(), {});
        history.clear();
        moves = 0;
        sel = -1; animating = false;
        updateMovesLabel();
        if (undoBtn) undoBtn->Enable(false);

        vector<int> blocks;
        for (int c = 0; c < nc; c++)
            for (int k = 0; k < CAP; k++)
                blocks.push_back(c);
        for (int i = (int)blocks.size()-1; i > 0; i--)
            swap(blocks[i], blocks[rand()%(i+1)]);
        for (int t = 0; t < nc; t++)
            for (int k = 0; k < CAP; k++)
                tubes[t].push_back(blocks[t*CAP+k]);
    }

    // Проверка возможности перелива
    bool canPour(int f, int t) {
        if (f==t || tubes[f].empty() || (int)tubes[t].size()>=CAP) return false;
        return tubes[t].empty() || tubes[f].back()==tubes[t].back();
    }

    // Переливание жидкости между трубками
    void pour(int f, int t) {
        int c = tubes[f].back();
        while (!tubes[f].empty() && tubes[f].back()==c && (int)tubes[t].size()<CAP) {
            tubes[t].push_back(c);
            tubes[f].pop_back();
        }
    }

    void startAnim(int f, int t) {
        saveState(); // сохраняем состояние до хода для undo
        animFrom  = f; animTo = t;
        animColor = tubes[f].back();
        animCount = 0;
        int space = CAP - (int)tubes[t].size();
        for (int i = (int)tubes[f].size()-1;
             i >= 0 && tubes[f][i] == animColor && animCount < space; i--)
            animCount++;
        animT = 0.f; animating = true;
        if (undoBtn) undoBtn->Enable(false); // блокируем undo во время анимации
        animTimer.Start(16);
    }

    void onAnimTimer(wxTimerEvent&) {
        animT += 0.055f;
        if (animT >= 1.f) {
            animTimer.Stop();
            animating = false;
            pour(animFrom, animTo);
            moves++;
            updateMovesLabel();
            if (undoBtn) undoBtn->Enable(!history.empty());
            sel = -1;
            panel->Refresh();
            if (won()) { showWin(); return; }
            if (!hasValidMoves()) showDeadlock();
            return;
        }
        panel->Refresh();
    }

    void showDeadlock() {
        wxString msg = ru("Нет доступных ходов!\n\n");
        if (!history.empty())
            msg += ru("Нажмите «Да» чтобы отменить последний ход,\n«Нет» чтобы начать заново.");
        else
            msg += ru("Нажмите «ОК» чтобы начать заново.");

        if (!history.empty()) {
            int ans = wxMessageBox(msg, ru("Тупик"), wxYES_NO | wxICON_WARNING, this);
            if (ans == wxYES) undo();
            else { init(); panel->Refresh(); }
        } else {
            wxMessageBox(msg, ru("Тупик"), wxOK | wxICON_WARNING, this);
            init(); panel->Refresh();
        }
    }

    void showWin() {
        string scoreKey = "Colbs_" + string(DIFFS[diffIndex].name.mb_str()) + "_Best";
        int best = LoadHighScore(scoreKey);
        bool newBest = (best == 0 || moves < best);
        if (newBest) SaveHighScore(scoreKey, moves);

        wxString msg = ru("Победа! [") + DIFFS[diffIndex].name + "]\n"
                     + ru("Ходов: ") + to_string(moves);
        if (newBest) msg += ru("\n*** Лучший результат! ***");
        else         msg += ru("\nРекорд: ") + to_string(best) + ru(" ходов");
        msg += ru("\n\nСыграть ещё раз?");

        int ans = wxMessageBox(msg, ru("Колбочки"), wxYES_NO|wxICON_INFORMATION, this);
        if (ans == wxYES) { init(); panel->Refresh(); }
        else goBack();
    }

    // Проверка условия победы
    bool won() {
        for (auto& tube : tubes) {
            if (tube.empty()) continue;
            if ((int)tube.size()!=CAP) return false;
            for (int x : tube) if (x!=tube[0]) return false;
        }
        return true;
    }

    bool hasValidMoves() {
        for (int f = 0; f < numTubes(); f++)
            for (int t = 0; t < numTubes(); t++)
                if (canPour(f, t)) return true;
        return false;
    }

    // Рисование трубки
    void drawTube(wxGraphicsContext* gctx, int i) {
        wxRect r  = tubeRect(i);
        int x=r.x, y=r.y, w=r.width, h=r.height;
        int cellH = h / CAP;
        bool selected = (i == sel);
        int lift = selected ? 18 : 0;

        // Сколько ячеек показывать (во время анимации убираем улетающие)
        int displaySize = (int)tubes[i].size();
        if (animating && i == animFrom)
            displaySize = max(0, displaySize - animCount);

        // Тень
        gctx->SetBrush(gctx->CreateRadialGradientBrush(x+w/2, y+h-lift+8, x+w/2, y+h-lift+8,
            w*0.7, wxColour(0,0,0,60), wxColour(0,0,0,0)));
        gctx->SetPen(*wxTRANSPARENT_PEN);
        gctx->DrawEllipse(x-5, y+h-lift, w+10, 14);

        // Жидкость
        for (int c = 0; c < displaySize; c++) {
            wxColour cellColor = pal(tubes[i][c]);
            int centY = y + h - (c+1)*cellH - lift;
            bool isBottom = (c == 0);
            bool isTop    = (c == displaySize-1);

            gctx->SetBrush(gctx->CreateLinearGradientBrush(x,centY,x+w,centY,
                cellColor.ChangeLightness(120), cellColor.ChangeLightness(80)));
            gctx->SetPen(*wxTRANSPARENT_PEN);

            if (isBottom && isTop) {
                gctx->DrawRoundedRectangle(x+3, centY+2, w-6, cellH-3, 6);
            } else if (isBottom) {
                gctx->DrawRoundedRectangle(x+3, centY, w-6, cellH+6, 6);
                gctx->SetBrush(gctx->CreateLinearGradientBrush(x,centY,x+w,centY,
                    cellColor.ChangeLightness(120), cellColor.ChangeLightness(80)));
                gctx->DrawRectangle(x+3, centY, w-6, 8);
            } else if (isTop) {
                gctx->DrawRoundedRectangle(x+3, centY+3, w-6, cellH-3, 3);
            } else {
                gctx->DrawRectangle(x+3, centY+1, w-6, cellH-1);
            }

            if (!isTop) {
                gctx->SetPen(gctx->CreatePen(wxGraphicsPenInfo(wxColour(0,0,0,50), 1)));
                gctx->StrokeLine(x+3, centY+cellH, x+w-3, centY+cellH);
            }
        }

        // Стекло
        wxColour glassStroke = selected ? wxColour(255,235,80) : wxColour(150,200,255,180);
        gctx->SetBrush(*wxTRANSPARENT_BRUSH);
        gctx->SetPen(gctx->CreatePen(wxGraphicsPenInfo(glassStroke, selected ? 2.5 : 1.5)));
        gctx->DrawRoundedRectangle(x, y-lift, w, h, 8);

        // Блик
        gctx->SetBrush(gctx->CreateLinearGradientBrush(x,y-lift,x+w/3,y-lift,
            wxColour(255,255,255,55), wxColour(255,255,255,0)));
        gctx->SetPen(*wxTRANSPARENT_PEN);
        gctx->DrawRoundedRectangle(x+2, y+2-lift, w/3-2, h-4, 5);

        // Гало при выделении
        if (selected) {
            gctx->SetBrush(*wxTRANSPARENT_BRUSH);
            gctx->SetPen(gctx->CreatePen(wxGraphicsPenInfo(wxColour(255,230,0,100), 8)));
            gctx->DrawRoundedRectangle(x-4, y-lift-4, w+8, h+8, 12);
        }
    }

    // Отрисовка графики на экране
    void onPaint(wxPaintEvent&) {
        wxAutoBufferedPaintDC dc(panel);
        wxSize panelSz = panel->GetSize();
        int panelW = panelSz.GetWidth(), panelH = panelSz.GetHeight();
        wxGraphicsContext* gctx = wxGraphicsContext::Create(dc);
        if (!gctx) return;
        gctx->SetAntialiasMode(wxANTIALIAS_DEFAULT);

        // Фон
        gctx->SetBrush(gctx->CreateLinearGradientBrush(0,0,0,panelH,
            wxColour(12,18,40), wxColour(22,35,70)));
        gctx->SetPen(*wxTRANSPARENT_PEN);
        gctx->DrawRectangle(0, 0, panelW, panelH);

        // Трубки
        for (int i = 0; i < numTubes(); i++) drawTube(gctx, i);

        // ── Анимация летящей жидкости ─────────────────────────────────────────
        if (animating) {
            float t = animT;
            wxRect rF = tubeRect(animFrom);
            wxRect rT = tubeRect(animTo);
            int cellH = rF.height / CAP;

            // Начальная точка: верх источника (минус улетевшие ячейки)
            int srcSize = (int)tubes[animFrom].size() - animCount;
            float sx = rF.x + rF.width  / 2.f;
            float sy = rF.y + rF.height - srcSize * cellH - 10.f;

            // Конечная точка: куда приземлится
            float ex = rT.x + rT.width  / 2.f;
            float ey = rT.y + rT.height - ((int)tubes[animTo].size() + animCount) * cellH + cellH/2.f;

            // Пик дуги
            float peakY = min(sy, ey) - 70.f;
            float midX  = (sx + ex) / 2.f;

            // Квадратичный Безье
            float boardX = (1-t)*(1-t)*sx + 2*(1-t)*t*midX + t*t*ex;
            float boardY = (1-t)*(1-t)*sy + 2*(1-t)*t*peakY + t*t*ey;

            // Хвост (несколько кружков с затуханием)
            for (int k = 3; k >= 0; k--) {
                float kt = max(0.f, t - k*0.07f);
                float kx = (1-kt)*(1-kt)*sx + 2*(1-kt)*kt*midX + kt*kt*ex;
                float ky = (1-kt)*(1-kt)*sy + 2*(1-kt)*kt*peakY + kt*kt*ey;
                float kr = 8.f - k*1.5f;
                unsigned char alpha = (unsigned char)(180 - k*40);
                wxColour cellColor = pal(animColor);
                gctx->SetBrush(gctx->CreateRadialGradientBrush(kx-kr/3,ky-kr/3, kx,ky, kr,
                    wxColour(cellColor.Red(),cellColor.Green(),cellColor.Blue(),alpha),
                    wxColour(cellColor.Red(),cellColor.Green(),cellColor.Blue(),0)));
                gctx->SetPen(*wxTRANSPARENT_PEN);
                gctx->DrawEllipse(kx-kr, ky-kr, kr*2, kr*2);
            }

            // Основной шар
            wxColour cellColor = pal(animColor);
            float boxRow = 11.f + animCount * 1.5f;
            gctx->SetBrush(gctx->CreateRadialGradientBrush(boardX-boxRow/3,boardY-boxRow/3, boardX,boardY, boxRow,
                cellColor.ChangeLightness(140), cellColor.ChangeLightness(75)));
            gctx->SetPen(gctx->CreatePen(wxGraphicsPenInfo(cellColor.ChangeLightness(60), 1)));
            gctx->DrawEllipse(boardX-boxRow, boardY-boxRow, boxRow*2, boxRow*2);
            // Блик
            gctx->SetBrush(gctx->CreateRadialGradientBrush(boardX-boxRow/3,boardY-boxRow/2.5,boardX,boardY,boxRow/2,
                wxColour(255,255,255,160), wxColour(255,255,255,0)));
            gctx->SetPen(*wxTRANSPARENT_PEN);
            gctx->DrawEllipse(boardX-boxRow/2,boardY-boxRow/1.5,boxRow,boxRow*0.7);
        }

        delete gctx;
    }

    // Обработка клика мыши
    void onClick(wxMouseEvent& e) {
        int hit = hitTest(e.GetX(), e.GetY());
        if (hit < 0) { sel=-1; panel->Refresh(); return; }

        if (sel < 0) {
            if (!tubes[hit].empty()) sel = hit;
        } else if (hit == sel) {
            sel = -1;
        } else if (canPour(sel, hit)) {
            startAnim(sel, hit); // запускаем анимацию вместо мгновенного переливания
            return;
        } else {
            sel = tubes[hit].empty() ? -1 : hit;
        }
        panel->Refresh();
    }

    // Перезапуск игры
    void onRestart(wxCommandEvent&) {
        animTimer.Stop(); animating = false;
        init(); panel->Refresh();
    }
    void onUndo(wxCommandEvent&) { undo(); }
    void onDiff(int d) {
        diffIndex = d;
        animTimer.Stop(); animating = false;
        updateDiffButtons();
        init(); panel->Refresh();
    }
    void onBack(wxCommandEvent&) { goBack(); }
    // Переход к меню игр
    void goBack();

public:
    // Конструктор класса gameColbs с инициализатором
    gameColbs() : wxFrame(nullptr, wxID_ANY, ru("Колбочки"), wxDefaultPosition, wxSize(800,600)),
                  animTimer(this), movesLabel(nullptr), undoBtn(nullptr)
    {
        panel = new wxPanel(this);
        panel->SetBackgroundStyle(wxBG_STYLE_PAINT);
        // Установка цвета фона
        panel->SetBackgroundColour(wxColour(12,18,40));

        // Левые 3 кнопки
        wxButton* backBtn = new wxButton(panel, wxID_ANY, ru("Меню"),
            wxPoint(8, 10), wxSize(88, 30));
        backBtn->Bind(wxEVT_BUTTON, &gameColbs::onBack, this);

        wxButton* restartBtn = new wxButton(panel, wxID_ANY, ru("Заново"),
            wxPoint(102, 10), wxSize(88, 30));
        restartBtn->Bind(wxEVT_BUTTON, &gameColbs::onRestart, this);

        undoBtn = new wxButton(panel, wxID_ANY, ru("Отмена"),
            wxPoint(196, 10), wxSize(88, 30));
        undoBtn->Bind(wxEVT_BUTTON, &gameColbs::onUndo, this);
        undoBtn->Enable(false);

        // Счётчик ходов по центру
        movesLabel = new wxStaticText(panel, wxID_ANY, ru("Ходов: 0"),
            wxPoint(360, 15), wxDefaultSize);
        // Установка цвета текста
        movesLabel->SetForegroundColour(*wxWHITE);
        movesLabel->SetFont(wxFont(13, wxFONTFAMILY_DEFAULT,
                                   wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD));

        // Правые 3 кнопки сложности
        wxString dnames[] = { ru("Легко"), ru("Норм."), ru("Сложно") };
        for (int d = 0; d < 3; d++) {
            diffBtns[d] = new wxButton(panel, wxID_ANY, dnames[d],
                wxPoint(516 + d*94, 10), wxSize(88, 30));
            diffBtns[d]->Bind(wxEVT_BUTTON, [this,d](wxCommandEvent&){ onDiff(d); });
        }

        wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
        sizer->Add(panel, 1, wxEXPAND);
        SetSizer(sizer);

        panel->Bind(wxEVT_PAINT,     &gameColbs::onPaint,     this);
        panel->Bind(wxEVT_LEFT_DOWN, &gameColbs::onClick,     this);
        Bind(wxEVT_TIMER,            &gameColbs::onAnimTimer, this, animTimer.GetId());

        updateDiffButtons();
        init();
    }
};

gameColbs::DiffConfig gameColbs::DIFFS[3] = {
    {4, 2, wxString(L"Легко")},       // Легко
    {6, 2, wxString(L"Нормально")}, // Нормально
    {8, 1, wxString(L"Сложно")}, // Сложно
};

// ─── Game settings ────────────────────────────────────────────────────────────
struct GameSettings {
    int      speed      = 150;
    wxColour snakeColor = *wxGREEN;
    wxColour foodColor  = *wxRED;
    wxColour bgColor    = *wxBLACK;
};

// ─── Settings dialog ──────────────────────────────────────────────────────────
// Класс SettingsDialog наследует wxDialog
class SettingsDialog : public wxDialog {
private:
    GameSettings& settings;
    wxRadioBox*   diffRadio;
    wxPanel*      snakePreview;
    wxPanel*      foodPreview;
    wxPanel*      bgPreview;

    // Обновление превью цветов в настройках
    void UpdatePreviews() {
        // Установка цвета фона
        snakePreview->SetBackgroundColour(settings.snakeColor);
        // Установка цвета фона
        foodPreview ->SetBackgroundColour(settings.foodColor);
        // Установка цвета фона
        bgPreview   ->SetBackgroundColour(settings.bgColor);
        snakePreview->Refresh();
        foodPreview ->Refresh();
        bgPreview   ->Refresh();
    }

    void PickColor(wxColour& target) {
        wxColourData data; data.SetColour(target);
        wxColourDialog dlg(this, &data);
        if (dlg.ShowModal() == wxID_OK) {
            target = dlg.GetColourData().GetColour();
            UpdatePreviews();
        }
    }

    void OnSnakeColor(wxCommandEvent&) { PickColor(settings.snakeColor); }
    void OnFoodColor (wxCommandEvent&) { PickColor(settings.foodColor);  }
    void OnBgColor   (wxCommandEvent&) { PickColor(settings.bgColor);    }

    void OnDifficulty(wxCommandEvent&) {
        switch (diffRadio->GetSelection()) {
            case 0: settings.speed = 220; break;
            case 1: settings.speed = 150; break;
            case 2: settings.speed =  80; break;
        }
    }

    void OnApply(wxCommandEvent&) { EndModal(wxID_OK); }

public:
    SettingsDialog(wxWindow* parent, GameSettings& s)
        : wxDialog(parent, wxID_ANY, ru("Настройки"),
                   wxDefaultPosition, wxSize(350, 420),
                   wxDEFAULT_DIALOG_STYLE),
          settings(s)
    {
        SetBackgroundColour(wxColour(30, 30, 30));
        wxBoxSizer* main = new wxBoxSizer(wxVERTICAL);

        wxString choices[] = { ru("Легко"), ru("Нормально"), ru("Сложно") };
        diffRadio = new wxRadioBox(this, wxID_ANY, ru("Сложность"),
            wxDefaultPosition, wxDefaultSize, 3, choices, 1, wxRA_SPECIFY_COLS);

        if      (settings.speed >= 220) diffRadio->SetSelection(0);
        else if (settings.speed >= 150) diffRadio->SetSelection(1);
        else                            diffRadio->SetSelection(2);

        // Установка цвета фона
        diffRadio->SetBackgroundColour(wxColour(30, 30, 30));
        // Установка цвета текста
        diffRadio->SetForegroundColour(*wxWHITE);
        diffRadio->Bind(wxEVT_RADIOBOX, &SettingsDialog::OnDifficulty, this);
        main->Add(diffRadio, 0, wxEXPAND | wxALL, 15);

        wxStaticText* visualLabel = new wxStaticText(this, wxID_ANY, ru("Визуальные"));
        // Установка цвета текста
        visualLabel->SetForegroundColour(*wxWHITE);
        visualLabel->SetFont(wxFont(13, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD));
        main->Add(visualLabel, 0, wxLEFT | wxTOP, 15);

        auto addColorRow = [&](const wxString& label, wxPanel*& preview,
                               void (SettingsDialog::*handler)(wxCommandEvent&))
        {
            wxBoxSizer* row = new wxBoxSizer(wxHORIZONTAL);
            wxButton* btn = new wxButton(this, wxID_ANY, label, wxDefaultPosition, wxSize(160, 35));
            btn->Bind(wxEVT_BUTTON, handler, this);
            preview = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxSize(35, 35));
            row->Add(btn,     0, wxALL, 5);
            row->Add(preview, 0, wxALL, 5);
            main->Add(row, 0, wxLEFT, 10);
        };

        addColorRow(ru("Цвет змейки"), snakePreview, &SettingsDialog::OnSnakeColor);
        addColorRow(ru("Цвет еды"),    foodPreview,  &SettingsDialog::OnFoodColor);
        addColorRow(ru("Цвет фона"),   bgPreview,    &SettingsDialog::OnBgColor);

        wxButton* applyBtn = new wxButton(this, wxID_ANY, ru("Применить"), wxDefaultPosition, wxSize(120, 35));
        // Установка цвета фона
        applyBtn->SetBackgroundColour(wxColour(0, 150, 0));
        // Установка цвета текста
        applyBtn->SetForegroundColour(*wxWHITE);
        applyBtn->Bind(wxEVT_BUTTON, &SettingsDialog::OnApply, this);
        main->Add(applyBtn, 0, wxALIGN_CENTER | wxALL, 15);

        SetSizer(main);
        UpdatePreviews();
    }
};

// ─── Snake game ───────────────────────────────────────────────────────────────
// Класс ThirdFrame наследует wxFrame
class ThirdFrame : public wxFrame {
private:
    enum class State { COUNTDOWN, RUNNING, PAUSED };

    int   Score     = 0;
    int   highScore = 0;
    int   baseSpeed = 150;
    State state     = State::COUNTDOWN;
    int   countdownVal = 3;

    wxPanel* panel;
    wxTimer  timer;
    wxTimer  countdownTimer;

    vector<wxPoint> snake;
    vector<wxPoint> obstacles;
    int dx = 1, dy = 0;
    wxPoint food;
    GameSettings settings;

    // ── Helpers ───────────────────────────────────────────────────────────────
    string DifficultyKey() {
        if (baseSpeed >= 220) return "Snake_Easy";
        if (baseSpeed >= 150) return "Snake_Normal";
        return "Snake_Hard";
    }

    bool IsCellFree(wxPoint p) {
        for (int i=0;i<(int)snake.size();i++)
            if (snake[i]==p) return false;
        for (int i=0;i<(int)obstacles.size();i++)
            if (obstacles[i]==p) return false;
        return true;
    }

    void GenerateFood() {
        int cols = panel->GetSize().GetWidth()  / 20;
        int rows = panel->GetSize().GetHeight() / 20;
        const int TOP = 3;
        do {
            food.x = rand() % cols;
            food.y = TOP + rand() % (rows - TOP);
        } while (!IsCellFree(food));
    }

    void GenerateObstacles() {
        obstacles.clear();
        if (baseSpeed > 80) return; // только Сложно
        int count = 10;
        int cols = panel->GetSize().GetWidth()  / 20;
        int rows = panel->GetSize().GetHeight() / 20;
        const int TOP = 3;
        auto nearStart = [](wxPoint p) { return p.y == 10 && p.x >= 6 && p.x <= 14; };
        int attempts = 0;
        while ((int)obstacles.size() < count && attempts < 1000) {
            wxPoint p(rand() % cols, TOP + rand() % (rows - TOP));
            if (!nearStart(p) && IsCellFree(p)) obstacles.push_back(p);
            attempts++;
        }
    }

    // ── Countdown ─────────────────────────────────────────────────────────────
    void StartCountdown() {
        state = State::COUNTDOWN;
        countdownVal = 3;
        timer.Stop();
        panel->Refresh();
        countdownTimer.Start(1000);
    }

    void OnCountdown(wxTimerEvent&) {
        countdownVal--;
        panel->Refresh();
        if (countdownVal < 0) {
            countdownTimer.Stop();
            state = State::RUNNING;
            timer.Start(settings.speed);
        }
    }

    // ── Game logic ────────────────────────────────────────────────────────────
    void OnTimer(wxTimerEvent&) {
        if (state != State::RUNNING) return;

        wxPoint headPos = snake.front();
        headPos.x += dx;
        headPos.y += dy;

        int cols = panel->GetSize().GetWidth()  / 20;
        int rows = panel->GetSize().GetHeight() / 20;

        if (headPos.x < 0 || headPos.x >= cols || headPos.y < 0 || headPos.y >= rows) {
            GameOver(); return;
        }
        for (size_t i = 1; i < snake.size(); i++) {
            if (headPos == snake[i]) { GameOver(); return; }
        }
        bool hitObs = false;
        for (int i = 0; i < (int)obstacles.size(); i++)
            if (obstacles[i] == headPos) { hitObs = true; break; }
        if (hitObs) { GameOver(); return; }

        snake.insert(snake.begin(), headPos);
        if (headPos == food) {
            Score++;
            GenerateFood();
            if (Score % 5 == 0 && settings.speed > 40) {
                settings.speed = max(40, settings.speed - 10);
                timer.Start(settings.speed);
            }
        } else {
            snake.pop_back();
        }
        panel->Refresh();
    }

    void OnKeyDown(wxKeyEvent& event) {
        if (event.GetKeyCode() == WXK_SPACE) {
            if (state == State::RUNNING) {
                state = State::PAUSED;
                timer.Stop();
                panel->Refresh();
            } else if (state == State::PAUSED) {
                state = State::RUNNING;
                timer.Start(settings.speed);
                panel->Refresh();
            }
            return;
        }
        if (state != State::RUNNING) { event.Skip(); return; }
        switch (event.GetKeyCode()) {
        case WXK_UP:    if (dy == 0) { dx =  0; dy = -1; } break;
        case WXK_DOWN:  if (dy == 0) { dx =  0; dy =  1; } break;
        case WXK_LEFT:  if (dx == 0) { dx = -1; dy =  0; } break;
        case WXK_RIGHT: if (dx == 0) { dx =  1; dy =  0; } break;
        default: event.Skip();
        }
    }

    void OnPaint(wxPaintEvent&) {
        wxAutoBufferedPaintDC dc(panel);
        wxSize size = panel->GetSize();
        int panelW = size.GetWidth(), panelH = size.GetHeight();

        wxGraphicsContext* gctx = wxGraphicsContext::Create(dc);
        if (!gctx) return;
        gctx->SetAntialiasMode(wxANTIALIAS_DEFAULT);

        // ── Фон ──────────────────────────────────────────────────────────────
        wxColour bg = settings.bgColor;
        gctx->SetBrush(gctx->CreateLinearGradientBrush(0,0,0,panelH,
            bg, wxColour(min(255,bg.Red()+20), min(255,bg.Green()+20), min(255,bg.Blue()+30))));
        gctx->SetPen(*wxTRANSPARENT_PEN);
        gctx->DrawRectangle(0, 0, panelW, panelH);

        // ── Сетка ─────────────────────────────────────────────────────────────
        gctx->SetPen(gctx->CreatePen(wxGraphicsPenInfo(
            wxColour(min(255,bg.Red()+15), min(255,bg.Green()+15), min(255,bg.Blue()+15)), 1)));
        for (int x = 0; x <= panelW; x += 20) gctx->StrokeLine(x,0,x,panelH);
        for (int y = 0; y <= panelH; y += 20) gctx->StrokeLine(0,y,panelW,y);

        // ── Препятствия ───────────────────────────────────────────────────────
        for (auto& obs : obstacles) {
            double ox = obs.x*20.0+2, oy = obs.y*20.0+2;
            gctx->SetBrush(gctx->CreateLinearGradientBrush(ox,oy,ox,oy+16,
                wxColour(160,80,0), wxColour(90,45,0)));
            gctx->SetPen(gctx->CreatePen(wxGraphicsPenInfo(wxColour(60,30,0), 1)));
            gctx->DrawRoundedRectangle(ox, oy, 16, 16, 5);
        }

        // ── Тело змейки ───────────────────────────────────────────────────────
        wxColour sc = settings.snakeColor;
        wxColour scDark = sc.ChangeLightness(60);
        for (int i = (int)snake.size()-1; i >= 1; i--) {
            double sx = snake[i].x*20.0+1, sy = snake[i].y*20.0+1;
            int light = 80 + (i % 3) * 8;
            gctx->SetBrush(gctx->CreateLinearGradientBrush(sx,sy, sx,sy+18,
                sc.ChangeLightness(light+15), sc.ChangeLightness(light)));
            gctx->SetPen(gctx->CreatePen(wxGraphicsPenInfo(scDark, 1)));
            gctx->DrawRoundedRectangle(sx, sy, 18, 18, 6);
        }

        // ── Голова ────────────────────────────────────────────────────────────
        if (!snake.empty()) {
            double hx = snake[0].x*20.0+1, hy = snake[0].y*20.0+1;
            gctx->SetBrush(gctx->CreateLinearGradientBrush(hx,hy,hx,hy+18,
                sc.ChangeLightness(140), sc.ChangeLightness(105)));
            gctx->SetPen(gctx->CreatePen(wxGraphicsPenInfo(scDark, 1.5)));
            gctx->DrawRoundedRectangle(hx, hy, 18, 18, 6);
            // глаза
            gctx->SetBrush(wxBrush(wxColour(20,20,20)));
            gctx->SetPen(*wxTRANSPARENT_PEN);
            int boardX = snake[0].x*20, boardY = snake[0].y*20;
            if      (dx== 1) { gctx->DrawEllipse(boardX+13,boardY+4,4,4); gctx->DrawEllipse(boardX+13,boardY+12,4,4); }
            else if (dx==-1) { gctx->DrawEllipse(boardX+3, boardY+4,4,4); gctx->DrawEllipse(boardX+3, boardY+12,4,4); }
            else if (dy==-1) { gctx->DrawEllipse(boardX+4, boardY+3,4,4); gctx->DrawEllipse(boardX+12,boardY+3, 4,4); }
            else             { gctx->DrawEllipse(boardX+4, boardY+13,4,4);gctx->DrawEllipse(boardX+12,boardY+13,4,4); }
            // зрачки (белые блики)
            gctx->SetBrush(wxBrush(*wxWHITE));
            if      (dx== 1) { gctx->DrawEllipse(boardX+14,boardY+5,2,2); gctx->DrawEllipse(boardX+14,boardY+13,2,2); }
            else if (dx==-1) { gctx->DrawEllipse(boardX+4, boardY+5,2,2); gctx->DrawEllipse(boardX+4, boardY+13,2,2); }
            else if (dy==-1) { gctx->DrawEllipse(boardX+5, boardY+4,2,2); gctx->DrawEllipse(boardX+13,boardY+4, 2,2); }
            else             { gctx->DrawEllipse(boardX+5, boardY+14,2,2);gctx->DrawEllipse(boardX+13,boardY+14,2,2); }
        }

        // ── Еда — глянцевый шар ───────────────────────────────────────────────
        {
            double fx = food.x*20.0, fy = food.y*20.0;
            wxColour fc = settings.foodColor;
            // тень
            gctx->SetBrush(gctx->CreateRadialGradientBrush(fx+11,fy+13,fx+10,fy+10,10,
                wxColour(0,0,0,80), wxColour(0,0,0,0)));
            gctx->SetPen(*wxTRANSPARENT_PEN);
            gctx->DrawEllipse(fx+2, fy+4, 18, 16);
            // шар
            gctx->SetBrush(gctx->CreateRadialGradientBrush(fx+8,fy+7, fx+10,fy+10, 9,
                fc.ChangeLightness(140), fc.ChangeLightness(70)));
            gctx->DrawEllipse(fx+2, fy+2, 16, 16);
            // блик
            gctx->SetBrush(gctx->CreateRadialGradientBrush(fx+7,fy+6, fx+7,fy+6, 4,
                wxColour(255,255,255,200), wxColour(255,255,255,0)));
            gctx->DrawEllipse(fx+4, fy+3, 8, 6);
        }

        // ── HUD ───────────────────────────────────────────────────────────────
        gctx->SetFont(wxFont(16, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD), *wxWHITE);
        gctx->DrawText("Score: " + to_string(Score), 10, 50);
        gctx->SetFont(wxFont(16, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD), wxColour(255,215,0));
        gctx->DrawText("Best:  " + to_string(highScore), 10, 75);
        wxString diff = (baseSpeed>=220)?ru("Легко"):(baseSpeed>=150)?ru("Нормально"):ru("Сложно");
        gctx->SetFont(wxFont(14, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL), wxColour(160,160,160));
        gctx->DrawText(diff, 10, 100);

        // ── Отсчёт ────────────────────────────────────────────────────────────
        if (state == State::COUNTDOWN) {
            wxString label = countdownVal > 0 ? to_string(countdownVal) : "GO!";
            wxFont bigFont(72, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD);
            gctx->SetFont(bigFont, wxColour(255,220,0));
            double textWidth, textHeight;
            gctx->GetTextExtent(label, &textWidth, &textHeight);
            gctx->DrawText(label, (panelW-textWidth)/2, (panelH-textHeight)/2);
        }

        // ── Пауза — полупрозрачный оверлей ────────────────────────────────────
        if (state == State::PAUSED) {
            gctx->SetBrush(wxBrush(wxColour(0,0,0,150)));
            gctx->SetPen(*wxTRANSPARENT_PEN);
            gctx->DrawRectangle(0, 0, panelW, panelH);
            wxFont pFont(48, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD);
            gctx->SetFont(pFont, *wxWHITE);
            double textWidth, textHeight;
            gctx->GetTextExtent(ru("ПАУЗА"), &textWidth, &textHeight);
            gctx->DrawText(ru("ПАУЗА"), (panelW-textWidth)/2, (panelH-textHeight)/2 - 35);
            wxFont hFont(16, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL);
            gctx->SetFont(hFont, wxColour(180,180,180));
            wxString hint = ru("Нажми ПРОБЕЛ для продолжения");
            gctx->GetTextExtent(hint, &textWidth, &textHeight);
            gctx->DrawText(hint, (panelW-textWidth)/2, (panelH-textHeight)/2 + 25);
        }

        delete gctx;
    }

    void OnSettings(wxCommandEvent&) {
        timer.Stop();
        countdownTimer.Stop();
        SettingsDialog dlg(this, settings);
        dlg.ShowModal();
        // Сбрасываем всё при смене настроек
        Score = 0;
        dx = 1; dy = 0;
        snake.clear();
        snake.push_back(wxPoint(10, 10));
        snake.push_back(wxPoint(9,  10));
        snake.push_back(wxPoint(8,  10));
        baseSpeed = settings.speed;
        highScore = LoadHighScore(DifficultyKey());
        GenerateObstacles();
        GenerateFood();
        // Установка цвета фона
        panel->SetBackgroundColour(settings.bgColor);
        StartCountdown();
    }

    void RestartGame() {
        Score = 0;
        dx = 1; dy = 0;
        settings.speed = baseSpeed;
        snake.clear();
        snake.push_back(wxPoint(10, 10));
        snake.push_back(wxPoint(9,  10));
        snake.push_back(wxPoint(8,  10));
        GenerateObstacles();
        GenerateFood();
        StartCountdown();
    }

    // Defined after SecondFrame below
    void OnBack(wxCommandEvent&);
    void GameOver();

    void OnExit(wxCommandEvent&) { Close(); }

public:
    // Конструктор класса ThirdFrame с инициализатором
    ThirdFrame() : wxFrame(nullptr, wxID_ANY, "Snake",
                            wxDefaultPosition, wxSize(800, 625)),
                   timer(this), countdownTimer(this)
    {
        SetMinSize(wxSize(800, 625));
        srand((unsigned)time(nullptr));
        SetBackgroundColour(wxColour(50, 50, 50));

        wxMenuBar* menuBar = new wxMenuBar();
        wxMenu* gameMenu = new wxMenu();
        gameMenu->Append(1, ru("Настройки"));
        gameMenu->Append(2, ru("Назад в меню"));
        gameMenu->AppendSeparator();
        gameMenu->Append(wxID_EXIT, ru("Выход"));
        menuBar->Append(gameMenu, ru("Меню"));
        SetMenuBar(menuBar);

        panel = new wxPanel(this, wxID_ANY);
        // Установка цвета фона
        panel->SetBackgroundColour(*wxBLACK);
        panel->SetBackgroundStyle(wxBG_STYLE_PAINT); // для wxAutoBufferedPaintDC

        wxButton* backBtn = new wxButton(panel, wxID_ANY, ru("Меню"),
            wxPoint(10, 10), wxSize(90, 30));
        backBtn->Bind(wxEVT_BUTTON, &ThirdFrame::OnBack, this);

        wxButton* settingsBtn = new wxButton(panel, wxID_ANY, ru("Настройки"),
            wxPoint(110, 10), wxSize(120, 30));
        settingsBtn->Bind(wxEVT_BUTTON, &ThirdFrame::OnSettings, this);

        wxBoxSizer* frameSizer = new wxBoxSizer(wxVERTICAL);
        frameSizer->Add(panel, 1, wxEXPAND);
        SetSizer(frameSizer);
        Layout();

        highScore = LoadHighScore(DifficultyKey());

        snake.push_back(wxPoint(10, 10));
        snake.push_back(wxPoint(9,  10));
        snake.push_back(wxPoint(8,  10));
        GenerateFood();

        Bind(wxEVT_TIMER,     &ThirdFrame::OnTimer,     this, timer.GetId());
        Bind(wxEVT_TIMER,     &ThirdFrame::OnCountdown, this, countdownTimer.GetId());
        Bind(wxEVT_CHAR_HOOK, &ThirdFrame::OnKeyDown,   this);
        Bind(wxEVT_MENU,      &ThirdFrame::OnSettings,  this, 1);
        Bind(wxEVT_MENU,      &ThirdFrame::OnBack,      this, 2);
        Bind(wxEVT_MENU,      &ThirdFrame::OnExit,      this, wxID_EXIT);
        panel->Bind(wxEVT_PAINT, &ThirdFrame::OnPaint,  this);

        CallAfter([this]() {
            panel->SetFocus();
            GenerateObstacles();
            GenerateFood();
            StartCountdown();
        });
    }
};

// ─── Game menu ────────────────────────────────────────────────────────────────
// Класс SecondFrame наследует wxFrame
class SecondFrame : public wxFrame {
public:
    // Конструктор класса SecondFrame с инициализатором
    SecondFrame() : wxFrame(nullptr, wxID_ANY, "GameVault",
                             wxDefaultPosition, wxSize(800, 600))
    {
        wxPanel* panel = new wxPanel(this);
        // Установка цвета фона
        panel->SetBackgroundColour(wxColour(20, 20, 40));
        wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);

        wxStaticText* title = new wxStaticText(panel, wxID_ANY, "GameVault",
            wxDefaultPosition, wxDefaultSize, wxALIGN_CENTER);
        // Установка цвета текста
        title->SetForegroundColour(*wxWHITE);
        title->SetFont(wxFont(26, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD));

        auto makeBtn = [&](const wxString& label) -> wxButton* {
            wxButton* b = new wxButton(panel, wxID_ANY, label,
                                       wxDefaultPosition, wxSize(160, 60));
            b->SetFont(wxFont(14, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD));
            return b;
        };

        wxButton* snake     = makeBtn("Snake");
        wxButton* colbs     = makeBtn("Colbs");
        wxButton* sudocu    = makeBtn("Sudoku");
        wxButton* breakfour = makeBtn(ru("Тетрис"));
        wxButton* toe       = makeBtn("Tic-Tac-Toe");
        wxButton* tag       = makeBtn("Tag");
        wxButton* labirint  = makeBtn("Labirint");
        wxButton* memory    = makeBtn("Memory");

        snake    ->Bind(wxEVT_BUTTON, &SecondFrame::OnSnakeClick,     this);
        colbs    ->Bind(wxEVT_BUTTON, &SecondFrame::OnColbsClick,     this);
        sudocu   ->Bind(wxEVT_BUTTON, &SecondFrame::OnSudocuClick,    this);
        breakfour->Bind(wxEVT_BUTTON, &SecondFrame::OnBreakFourClick, this);
        toe      ->Bind(wxEVT_BUTTON, &SecondFrame::OnTicTacToeClick, this);
        tag      ->Bind(wxEVT_BUTTON, &SecondFrame::OnTagClick,       this);
        labirint ->Bind(wxEVT_BUTTON, &SecondFrame::OnLabirintClick,  this);
        memory   ->Bind(wxEVT_BUTTON, &SecondFrame::OnMemoryClick,    this);

        wxGridSizer* grid = new wxGridSizer(2, 4, 15, 15);
        grid->Add(snake,     0, wxEXPAND);
        grid->Add(colbs,     0, wxEXPAND);
        grid->Add(sudocu,    0, wxEXPAND);
        grid->Add(breakfour, 0, wxEXPAND);
        grid->Add(toe,       0, wxEXPAND);
        grid->Add(tag,       0, wxEXPAND);
        grid->Add(labirint,  0, wxEXPAND);
        grid->Add(memory,    0, wxEXPAND);

        mainSizer->Add(title, 0, wxALIGN_CENTER | wxTOP | wxBOTTOM, 20);
        mainSizer->Add(grid,  1, wxEXPAND | wxALL, 20);
        panel->SetSizer(mainSizer);
    }

private:
    template<typename T>
    void OpenGame(wxCommandEvent&) { T* g = new T(); g->Show(); Close(); }

    void OnSnakeClick    (wxCommandEvent& e) { OpenGame<ThirdFrame>   (e); }
    void OnColbsClick    (wxCommandEvent& e) { OpenGame<gameColbs>    (e); }
    void OnSudocuClick   (wxCommandEvent& e) { OpenGame<gameSudocu>   (e); }
    void OnBreakFourClick(wxCommandEvent& e) { OpenGame<gameBreakFour>(e); }
    void OnTicTacToeClick(wxCommandEvent& e) { OpenGame<gameTicTac>   (e); }
    void OnTagClick      (wxCommandEvent& e) { OpenGame<gameTag>      (e); }
    void OnLabirintClick (wxCommandEvent& e) { OpenGame<gameLabirint> (e); }
    void OnMemoryClick   (wxCommandEvent& e) { OpenGame<gamememory>   (e); }
};

// ─── Out-of-line definitions (SecondFrame fully defined) ─────────────────────
void gamememory::goBack() {
    SecondFrame* menu = new SecondFrame();
    menu->Show();
    Close();
}

void gameTicTac::onBack(wxCommandEvent&) {
    SecondFrame* menu = new SecondFrame();
    menu->Show();
    Close();
}

void gameBreakFour::onBack(wxCommandEvent&) {
    SecondFrame* menu = new SecondFrame();
    menu->Show();
    Close();
}

void gameSudocu::onBack(wxCommandEvent&) {
    SecondFrame* menu = new SecondFrame();
    menu->Show();
    Close();
}

void gameColbs::goBack() {
    SecondFrame* menu = new SecondFrame();
    menu->Show();
    Close();
}
void ThirdFrame::GameOver() {
    timer.Stop();
    bool newRecord = Score > highScore;
    if (newRecord) {
        highScore = Score;
        SaveHighScore(DifficultyKey(), highScore);
    }
    wxString diff = (baseSpeed >= 220) ? ru("Легко") : (baseSpeed >= 150) ? ru("Нормально") : ru("Сложно");
    wxString msg = ru("Игра окончена!  [") + diff + "]\n"
                 + ru("Счёт: ") + to_string(Score)
                 + ru("\nРекорд: ") + to_string(highScore);
    if (newRecord) msg += ru("\n\n*** Новый рекорд! ***");
    msg += ru("\n\nСыграть ещё раз?");

    int answer = wxMessageBox(msg, "Snake", wxYES_NO | wxICON_QUESTION, this);
    if (answer == wxYES) {
        RestartGame();
    } else {
        SecondFrame* menu = new SecondFrame();
        menu->Show();
        Close();
    }
}

void StubGame::OnBack(wxCommandEvent&) {
    SecondFrame* menu = new SecondFrame();
    menu->Show();
    Close();
}

void ThirdFrame::OnBack(wxCommandEvent&) {
    timer.Stop();
    countdownTimer.Stop();
    SecondFrame* menu = new SecondFrame();
    menu->Show();
    Close();
}

// ─── Start screen ─────────────────────────────────────────────────────────────
// Класс MyFrame наследует wxFrame
class MyFrame : public wxFrame {
public:
    // Конструктор класса MyFrame с инициализатором
    MyFrame() : wxFrame(nullptr, wxID_ANY, "GameVault",
                         wxDefaultPosition, wxSize(800, 600))
    {
        wxPanel* panel = new wxPanel(this);
        // Установка цвета фона
        panel->SetBackgroundColour(wxColour(10, 10, 25));
        wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);

        wxStaticText* title = new wxStaticText(panel, wxID_ANY, "GameVault",
            wxDefaultPosition, wxDefaultSize, wxALIGN_CENTER);
        // Установка цвета текста
        title->SetForegroundColour(*wxWHITE);
        title->SetFont(wxFont(36, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD));

        wxButton* playBtn = new wxButton(panel, wxID_ANY, ru("  Играть  "),
            wxDefaultPosition, wxSize(250, 80));
        playBtn->SetFont(wxFont(28, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD));
        playBtn->Bind(wxEVT_BUTTON, &MyFrame::OnPlayClick, this);

        sizer->AddStretchSpacer(1);
        sizer->Add(title,   0, wxALIGN_CENTER | wxALL, 20);
        sizer->Add(playBtn, 0, wxALIGN_CENTER | wxALL, 30);
        sizer->AddStretchSpacer(1);
        panel->SetSizer(sizer);
    }

private:
    void OnPlayClick(wxCommandEvent&) {
        SecondFrame* second = new SecondFrame();
        second->Show();
        Close();
    }
};

// ─── App ──────────────────────────────────────────────────────────────────────
// Класс MyApp наследует wxApp
class MyApp : public wxApp {
public:
    // Инициализация приложения
    bool OnInit() override {
        srand((unsigned)time(nullptr));
        MyFrame* frame = new MyFrame();
        frame->Show(true);
        return true;
    }
};

wxIMPLEMENT_APP(MyApp);