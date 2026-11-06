#pragma once
#include "FlagArenaDriver.h"
#include <vector>
#include <queue>
#include <limits>
namespace TwinKartRace {
// Independently built height-field graph. Samples come from the owned game's
// road collision query; no copied navigation asset or artificial vehicle pose.
class FlagGroundNavigation {
    struct Cell {ArenaPoint point{};bool valid=false;unsigned short blocked=0;};
    std::vector<Cell> cells;int width=0,height=0;float x0=0,y0=0,spacing=400;size_t sampled=0;
    mutable std::vector<int> regions;
    mutable bool regionsReady=false;
    bool Edge(int a,int b)const {
        if(width<1)return false;
        int dx=b%width-a%width,dy=b/width-a/width;
        return a>=0 && b>=0 && static_cast<size_t>(a)<cells.size() && static_cast<size_t>(b)<cells.size() &&
            std::abs(dx)<=1 && std::abs(dy)<=1 && !(cells[a].blocked&(1u<<((dy+1)*3+dx+1))) && cells[a].valid && cells[b].valid &&
            // A continuous collision surface can still be a steep bank beside
            // the usable ramp. Karts cannot reliably climb those side faces.
            std::fabs(cells[a].point.z-cells[b].point.z)<=ArenaDistance(cells[a].point,cells[b].point)*0.25f+15;
    }
    bool Visible(int a,int b)const {
        float ax=static_cast<float>(a%width),ay=static_cast<float>(a/width),bx=static_cast<float>(b%width),by=static_cast<float>(b/width);
        int samples=static_cast<int>(std::ceil((std::max)(std::fabs(bx-ax),std::fabs(by-ay))*3));
        int previous=a;
        for(int i=1;i<=samples;++i) {
            float t=static_cast<float>(i)/samples;int x=static_cast<int>(std::round(ax+(bx-ax)*t)),y=static_cast<int>(std::round(ay+(by-ay)*t));
            int next=y*width+x;if(next==previous)continue;if(!Edge(previous,next))return false;
            int px=previous%width,py=previous/width;
            if(x!=px && y!=py && (!Edge(previous,py*width+x) || !Edge(previous,y*width+px)))return false;
            previous=next;
        }
        return true;
    }
    int Nearest(const ArenaPoint& point,bool(*line)(const ArenaPoint&,const ArenaPoint&,void*)=nullptr,void* context=nullptr)const {
        int indices[16]{};float scores[16]{};int used=0;
        for(size_t i=0;i<cells.size();++i)if(cells[i].valid && std::fabs(point.z-cells[i].point.z)<400) {
            float d=ArenaDistance(point,cells[i].point)+4*std::fabs(point.z-cells[i].point.z);
            if(d>=1800)continue;
            int at=0;while(at<used && scores[at]<=d)++at;if(at>=16)continue;
            if(used<16)++used;
            for(int next=used-1;next>at;--next){indices[next]=indices[next-1];scores[next]=scores[next-1];}
            indices[at]=static_cast<int>(i);scores[at]=d;
        }
        for(int i=0;i<used;++i)if(!line || line(point,cells[indices[i]].point,context))return indices[i];
        return -1;
    }
    void BuildRegions()const {
        if(regionsReady || !Ready())return;
        regions.assign(cells.size(),-1);int region=0;std::vector<int> pending;
        for(size_t first=0;first<cells.size();++first)if(cells[first].valid && regions[first]<0) {
            pending.clear();pending.push_back(static_cast<int>(first));regions[first]=region;
            while(!pending.empty()) {
                int at=pending.back();pending.pop_back();int x=at%width,y=at/width;
                for(int dy=-1;dy<=1;++dy)for(int dx=-1;dx<=1;++dx) {
                    if((!dx&&!dy)||x+dx<0||x+dx>=width||y+dy<0||y+dy>=height)continue;
                    int to=at+dx+dy*width;if(regions[to]>=0 || !Edge(at,to))continue;
                    if(dx && dy && (!Edge(at,at+dx) || !Edge(at,at+dy*width)))continue;
                    regions[to]=region;pending.push_back(to);
                }
            }
            ++region;
        }
        regionsReady=true;
    }
public:
    void Reset(){cells.clear();regions.clear();regionsReady=false;width=height=0;sampled=0;}
    bool Initialize(const ArenaPoint& low,const ArenaPoint& high,float step=400) {
        Reset();if(!Finite(low)||!Finite(high)||step<100 || step>1000 || high.x<=low.x || high.y<=low.y)return false;
        int w=static_cast<int>(std::ceil((high.x-low.x)/step))+1,h=static_cast<int>(std::ceil((high.y-low.y)/step))+1;
        if(w<2 || h<2 || w>192 || h>192 || w*h>20000)return false;
        width=w;height=h;x0=low.x;y0=low.y;spacing=step;cells.resize(static_cast<size_t>(w*h));return true;
    }
    bool Started()const{return !cells.empty();}
    // Retained formal arenas can be much larger than development maps. Keep
    // the same bounded cell/axis budgets and actual collision sampling, while
    // relaxing sample spacing explicitly instead of retrying a rejected grid
    // on every game tick. Legacy Initialize callers retain their exact step.
    bool InitializeBounded(const ArenaPoint& low,const ArenaPoint& high,float preferred=300) {
        if(!std::isfinite(preferred) || preferred<100 || preferred>1000){Reset();return false;}
        for(float step=preferred;;step=(std::min)(1000.f,step+50)) {
            if(Initialize(low,high,step))return true;
            if(step>=1000)return false;
        }
    }
    bool Ready()const{return Started() && sampled==cells.size();}
    size_t Count()const{return cells.size();}
    int RegionAtSample(size_t index)const {
        if(!Ready() || index>=cells.size())return -1;BuildRegions();return regions[index];
    }
    int RegionAt(const ArenaPoint& point,bool(*line)(const ArenaPoint&,const ArenaPoint&,void*)=nullptr,void* context=nullptr)const {
        if(!Ready() || !Finite(point))return -1;int index=Nearest(point,line,context);
        return index<0?-1:RegionAtSample(static_cast<size_t>(index));
    }
    bool ReachableSampleAt(size_t index,int region,ArenaPoint& point,
        bool(*line)(const ArenaPoint&,const ArenaPoint&,void*)=nullptr,void* context=nullptr)const {
        ArenaPoint candidate{};
        if(region<0 || RegionAtSample(index)!=region || !SampleAt(index,candidate) || RegionAt(candidate,line,context)!=region)return false;
        point=candidate;return true;
    }
    size_t AdjacencyCount()const{return Ready()?cells.size()*4:0;}
    bool AdjacencyAt(size_t index,int& a,int& b,ArenaPoint& first,ArenaPoint& second)const {
        if(index>=AdjacencyCount())return false;
        constexpr int dx[]{1,0,1,-1},dy[]{0,1,1,1};
        a=static_cast<int>(index/4);int direction=static_cast<int>(index%4);
        int x=a%width+dx[direction],y=a/width+dy[direction];
        if(x<0 || x>=width || y>=height)return false;b=y*width+x;
        if(!Edge(a,b))return false;first=cells[a].point;second=cells[b].point;return true;
    }
    bool BlockTransition(int a,int b) {
        if(a<0 || b<0 || static_cast<size_t>(a)>=cells.size() || static_cast<size_t>(b)>=cells.size() || a==b)return false;
        int dx=b%width-a%width,dy=b/width-a/width;if(std::abs(dx)>1 || std::abs(dy)>1)return false;
        cells[a].blocked|=static_cast<unsigned short>(1u<<((dy+1)*3+dx+1));
        cells[b].blocked|=static_cast<unsigned short>(1u<<((-dy+1)*3-dx+1));regionsReady=false;return true;
    }
    bool SampleAt(size_t index,ArenaPoint& point)const {
        if(index>=sampled || !cells[index].valid)return false;
        point=cells[index].point;return true;
    }
    size_t ValidCount()const{size_t n=0;for(const auto& c:cells)if(c.valid)++n;return n;}
    bool Next(ArenaPoint& point)const {
        if(!Started() || Ready())return false;
        point={x0+static_cast<float>(sampled%width)*spacing,y0+static_cast<float>(sampled/width)*spacing,0};return true;
    }
    void Sample(float z,bool valid) {
        if(!Started() || Ready())return;
        auto& c=cells[sampled];c.point={x0+static_cast<float>(sampled%width)*spacing,y0+static_cast<float>(sampled/width)*spacing,z};
        c.valid=valid && std::isfinite(z);++sampled;
    }
    bool Path(const ArenaPoint& start,const ArenaPoint& goal,std::vector<ArenaPoint>& output,
        bool(*line)(const ArenaPoint&,const ArenaPoint&,void*)=nullptr,void* context=nullptr)const {
        output.clear();if(!Ready() || !Finite(start) || !Finite(goal))return false;
        int first=Nearest(start,line,context),last=Nearest(goal,line,context);if(first<0 || last<0)return false;
        struct Node {int index;float cost;bool operator<(const Node& b)const{return cost>b.cost;}};
        std::priority_queue<Node> open;std::vector<float> cost(cells.size(),1.e30f);std::vector<int> parent(cells.size(),-1);
        std::vector<bool> closed(cells.size(),false);cost[first]=0;open.push({first,0});
        while(!open.empty()) {
            int at=open.top().index;open.pop();if(closed[at])continue;closed[at]=true;
            if(at==last)break;
            int x=at%width,y=at/width;
            for(int dy=-1;dy<=1;++dy)for(int dx=-1;dx<=1;++dx) {
                if((!dx&&!dy)||x+dx<0||x+dx>=width||y+dy<0||y+dy>=height)continue;
                int to=at+dx+dy*width;if(closed[to] || !Edge(at,to))continue;
                if(dx && dy && (!Edge(at,at+dx) || !Edge(at,at+dy*width)))continue;
                float step=ArenaDistance(cells[at].point,cells[to].point)+std::fabs(cells[at].point.z-cells[to].point.z);
                // Prefer the middle of a road when an equivalent edge route
                // would put the vehicle beside a wall or platform boundary.
                unsigned borders=0;
                int tx=to%width,ty=to/width;
                for(int sy=-1;sy<=1;++sy)for(int sx=-1;sx<=1;++sx)
                    if(tx+sx<0||tx+sx>=width||ty+sy<0||ty+sy>=height || !Edge(to,to+sx+sy*width))++borders;
                float candidate=cost[at]+step+150.f*borders;if(candidate>=cost[to])continue;
                cost[to]=candidate;parent[to]=at;open.push({to,candidate+ArenaDistance(cells[to].point,cells[last].point)});
            }
        }
        if(!closed[last])return false;
        std::vector<int> reverse;for(int at=last;at>=0;at=parent[at]){reverse.push_back(at);if(at==first)break;}
        if(reverse.empty() || reverse.back()!=first)return false;
        std::vector<int> ordered(reverse.rbegin(),reverse.rend());
        size_t at=0;output.push_back(cells[ordered[0]].point);
        while(at+1<ordered.size()) {
            size_t next=(std::min)(at+12,ordered.size()-1);
            while(next>at+1 && !Visible(ordered[at],ordered[next]))--next;
            output.push_back(cells[ordered[next]].point);at=next;
        }
        output.push_back(goal);return true;
    }
};
struct FlagGroundRoute {
    std::vector<ArenaPoint> points;ArenaPoint goal{};size_t next=0;unsigned long long planned=0;
    void Reset(){points.clear();goal={};next=0;planned=0;}
    ArenaPoint Target(const FlagGroundNavigation& field,const ArenaPoint& position,const ArenaPoint& desired,unsigned long long now) {
        if(!planned || ArenaDistance(goal,desired)>1000 || std::fabs(goal.z-desired.z)>400 || now-planned>3000) {
            planned=now;goal=desired;next=0;field.Path(position,desired,points);
        }
        while(next<points.size() && ArenaDistance(position,points[next])<450 && std::fabs(position.z-points[next].z)<300)++next;
        if(next>=points.size())return desired;
        // Follow two cells ahead on a straight portion only. Keep corner and
        // height changes so a shortcut cannot jump from a lower road to a roof.
        size_t look=next;
        if(next+1<points.size()) {
            const auto& a=points[next];const auto& b=points[next+1];
            float ax=a.x-position.x,ay=a.y-position.y,bx=b.x-a.x,by=b.y-a.y;
            float cross=std::fabs(ax*by-ay*bx),scale=std::hypot(ax,ay)*std::hypot(bx,by);
            if(scale>1 && cross/scale<0.15f && std::fabs(a.z-b.z)<80)look=next+1;
        }
        return points[look];
    }
};
}
