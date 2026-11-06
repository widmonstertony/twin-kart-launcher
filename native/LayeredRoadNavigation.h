#pragma once
#include "NativeRoadQuery.h"
#include <vector>
#include <queue>
#include <limits>
namespace TwinKartRace {
// Original sampled-road navigation with independent nodes for stacked road
// surfaces. No same-XY vertical links exist. Candidate links still require
// the caller's native collision proof before any route can be returned.
class LayeredRoadNavigation {
    struct Column {int first=-1,count=0;};
    struct Node {ArenaPoint point{};int column=0;std::vector<int> links;};
    struct Edge {int a=0,b=0;int state=-1;};
    std::vector<Column> columns;std::vector<Node> nodes;std::vector<Edge> edges;
    mutable std::vector<int> regions;mutable bool regionsReady=false;
    int width=0,height=0;float x0=0,y0=0,spacing=400,top=0,bottom=0,cursor=0;
    size_t column=0,edgeColumn=0,pending=0;unsigned columnRays=0;int rejected=0;
    bool edgesBuilt=false,smoothRoutes=true;
    void Reject(int reason){rejected=reason;regionsReady=false;}
    void Advance(){++column;columnRays=0;cursor=top;}
    bool Candidate(int a,int b)const {
        if(a<0 || b<0 || a==b || static_cast<size_t>(a)>=nodes.size() || static_cast<size_t>(b)>=nodes.size())return false;
        int ac=nodes[a].column,bc=nodes[b].column;
        if(std::abs(ac%width-bc%width)+std::abs(ac/width-bc/width)!=1)return false;
        return std::fabs(nodes[a].point.z-nodes[b].point.z)<=spacing*0.60f+15;
    }
    bool Connected(int a,int b)const {
        if(a<0 || b<0 || static_cast<size_t>(a)>=nodes.size() || static_cast<size_t>(b)>=nodes.size())return false;
        for(int index:nodes[a].links) {const auto& e=edges[index];if(e.state==1 && (e.a==b || e.b==b))return true;}
        return false;
    }
    bool Corner(int a,int b,int cell,float z)const {
        if(cell<0 || static_cast<size_t>(cell)>=columns.size())return false;
        const auto& c=columns[cell];
        for(int k=0;k<c.count;++k) {
            int n=c.first+k;
            if(std::fabs(nodes[n].point.z-z)<200 && Connected(a,n) && Connected(n,b))return true;
        }
        return false;
    }
    bool Visible(int a,int b,bool(*line)(const ArenaPoint&,const ArenaPoint&,void*),void* context)const {
        const auto& start=nodes[a];const auto& end=nodes[b];
        if(line && !line(start.point,end.point,context))return false;
        float ax=static_cast<float>(start.column%width),ay=static_cast<float>(start.column/width);
        float bx=static_cast<float>(end.column%width),by=static_cast<float>(end.column/width);
        int samples=static_cast<int>(std::ceil((std::max)(std::fabs(ax-bx),std::fabs(ay-by))*3));
        if(!samples)return a==b;
        int previous=a;
        for(int i=1;i<=samples;++i) {
            float t=static_cast<float>(i)/samples;
            int x=static_cast<int>(std::round(ax+(bx-ax)*t)),y=static_cast<int>(std::round(ay+(by-ay)*t)),cell=y*width+x;
            int old=nodes[previous].column;if(cell==old)continue;
            if(cell<0 || static_cast<size_t>(cell)>=columns.size())return false;
            int next=-1;float best=1.e30f,z=start.point.z+(end.point.z-start.point.z)*t;
            const auto& c=columns[cell];int px=old%width,py=old/width;
            for(int k=0;k<c.count;++k) {
                int candidate=c.first+k;float dz=std::fabs(nodes[candidate].point.z-z);if(dz>=200 || dz>=best)continue;
                bool linked=px==x || py==y?Connected(previous,candidate):
                    Corner(previous,candidate,py*width+x,z) && Corner(previous,candidate,y*width+px,z);
                if(linked){next=candidate;best=dz;}
            }
            if(next<0)return false;previous=next;
        }
        return previous==b;
    }
    int Nearest(const ArenaPoint& p,bool(*line)(const ArenaPoint&,const ArenaPoint&,void*),void* context,
        bool(*connect)(const ArenaPoint&,const ArenaPoint&,void*)=nullptr,void* connectionContext=nullptr,bool towardPoint=false)const {
        int indices[16]{};float scores[16]{};int used=0;
        for(size_t i=0;i<nodes.size();++i) {
            const auto& q=nodes[i].point;float dz=p.z-q.z;
            // Native actor roots may sit above their road. Do not snap a
            // below-bridge pawn upward onto a deck above its observed pose.
            if(dz< -40 || dz>300)continue;
            float distance=ArenaDistance(p,q);if(distance>=1800)continue;
            float cost=distance+6*std::fabs(dz);int at=0;while(at<used && scores[at]<=cost)++at;if(at>=16)continue;
            if(used<16)++used;
            for(int next=used-1;next>at;--next){indices[next]=indices[next-1];scores[next]=scores[next-1];}
            indices[at]=static_cast<int>(i);scores[at]=cost;
        }
        for(int i=0;i<used;++i) {
            const auto& node=nodes[indices[i]].point;
            if(line && !line(p,node,context))continue;
            if(connect && ArenaDistance(p,node)>0.01f &&
               !connect(towardPoint?node:p,towardPoint?p:node,connectionContext))continue;
            return indices[i];
        }
        return -1;
    }
    void Regions()const {
        if(regionsReady || !Verified())return;
        regions.assign(nodes.size(),-1);int region=0;std::vector<int> todo;
        for(size_t first=0;first<nodes.size();++first)if(regions[first]<0) {
            todo.clear();todo.push_back(static_cast<int>(first));regions[first]=region;
            while(!todo.empty()) {
                int at=todo.back();todo.pop_back();
                for(int index:nodes[at].links) {
                    const auto& e=edges[index];if(e.state!=1)continue;int to=e.a==at?e.b:e.a;
                    if(regions[to]<0){regions[to]=region;todo.push_back(to);}
                }
            }
            ++region;
        }
        regionsReady=true;
    }
public:
    void Reset(){columns.clear();nodes.clear();edges.clear();regions.clear();regionsReady=false;width=height=0;
        x0=y0=top=bottom=cursor=0;spacing=400;column=edgeColumn=pending=0;columnRays=0;rejected=0;edgesBuilt=false;}
    bool Initialize(const ArenaPoint& low,const ArenaPoint& high,float step=400) {
        Reset();
        if(!Finite(low) || !Finite(high) || !std::isfinite(step) || step<100 || step>1000 ||
           high.x<=low.x || high.y<=low.y || high.z<=low.z){Reject(1);return false;}
        double w=std::ceil((static_cast<double>(high.x)-low.x)/step)+1,h=std::ceil((static_cast<double>(high.y)-low.y)/step)+1;
        if(w<2 || h<2 || w>192 || h>192 || w*h>20000){Reject(1);return false;}
        width=static_cast<int>(w);height=static_cast<int>(h);x0=low.x;y0=low.y;spacing=step;bottom=low.z;top=cursor=high.z;
        columns.resize(static_cast<size_t>(width*height));return true;
    }
    bool Started()const{return !columns.empty();}
    bool InitializeBounded(const ArenaPoint& low,const ArenaPoint& high,float preferred=300) {
        if(!std::isfinite(preferred) || preferred<100 || preferred>1000){Reset();return false;}
        for(float step=preferred;;step=(std::min)(1000.f,step+50)) {
            if(Initialize(low,high,step))return true;
            if(step>=1000)return false;
        }
    }
    // Native callers can retain every individually support-checked edge.
    // Graph-connected corners alone do not prove support along a shortcut.
    void AllowSmoothing(bool value){smoothRoutes=value;}
    bool Rejected()const{return rejected!=0;}
    int Rejection()const{return rejected;}
    bool SamplingDone()const{return Started() && column==columns.size() && !Rejected();}
    bool Ready()const{return SamplingDone() && edgesBuilt;}
    bool Verified()const{return Ready() && !pending;}
    size_t Count()const{return nodes.size();}
    size_t ValidCount()const{return nodes.size();}
    size_t ColumnCount()const{return columns.size();}
    size_t SampledColumnCount()const{return column;}
    size_t PendingEdgeCount()const{return pending;}
    bool NextRay(ArenaPoint& start,ArenaPoint& end)const {
        if(!Started() || column>=columns.size() || Rejected())return false;
        start={x0+static_cast<float>(column%width)*spacing,y0+static_cast<float>(column/width)*spacing,cursor};end=start;end.z=bottom;return true;
    }
    void Sample(RoadSurfaceResult result,const RoadSurface& surface) {
        if(!Started() || column>=columns.size() || Rejected())return;
        if((result!=RoadSurfaceResult::Hit && result!=RoadSurfaceResult::Miss) || ++columnRays>512){Reject(2);return;}
        if(result==RoadSurfaceResult::Miss){Advance();return;}
        ArenaPoint start{},end{};NextRay(start,end);
        if(!Finite(surface.point) || !std::isfinite(surface.normalZ) || std::fabs(surface.normalZ)>1.01f ||
           std::fabs(surface.point.x-start.x)>2 || std::fabs(surface.point.y-start.y)>2 ||
           surface.point.z>cursor+2 || surface.point.z<bottom-2){Reject(3);return;}
        if(surface.normalZ>0.70f && !surface.penetrating) {
            auto& c=columns[column];bool duplicate=false;
            for(int i=0;i<c.count;++i)duplicate|=std::fabs(nodes[c.first+i].point.z-surface.point.z)<4;
            if(!duplicate) {
                if(c.count>=16 || nodes.size()>=80000){Reject(4);return;}
                if(!c.count)c.first=static_cast<int>(nodes.size());
                nodes.push_back({{start.x,start.y,surface.point.z},static_cast<int>(column),{}});++c.count;
            }
        }
        float next=surface.point.z-2;
        if(surface.penetrating || next>=cursor-1)next=cursor-16;
        if(!std::isfinite(next) || next>=cursor){Reject(3);return;}
        cursor=next;if(cursor<=bottom)Advance();
    }
    bool BuildEdges(unsigned budget) {
        if(!SamplingDone() || !budget || budget>256 || Rejected())return false;
        for(unsigned work=0;work<budget && edgeColumn<columns.size();++work,++edgeColumn) {
            int x=static_cast<int>(edgeColumn)%width,y=static_cast<int>(edgeColumn)/width;const auto& c=columns[edgeColumn];
            for(int direction=0;direction<2;++direction) {
                int other=direction==0?(x+1<width?static_cast<int>(edgeColumn)+1:-1):(y+1<height?static_cast<int>(edgeColumn)+width:-1);
                if(other<0)continue;const auto& d=columns[other];
                for(int i=0;i<c.count;++i)for(int j=0;j<d.count;++j) {
                    int a=c.first+i,b=d.first+j;if(!Candidate(a,b))continue;
                    if(edges.size()>=640000){Reject(5);return false;}
                    int index=static_cast<int>(edges.size());edges.push_back({a,b,-1});nodes[a].links.push_back(index);nodes[b].links.push_back(index);++pending;
                }
            }
        }
        edgesBuilt=edgeColumn==columns.size();return edgesBuilt;
    }
    size_t AdjacencyCount()const{return Ready()?edges.size():0;}
    bool AdjacencyAt(size_t index,int& a,int& b,ArenaPoint& first,ArenaPoint& second)const {
        if(!Ready() || index>=edges.size())return false;
        const auto& e=edges[index];a=e.a;b=e.b;first=nodes[a].point;second=nodes[b].point;return true;
    }
    bool ResolveTransition(size_t index,bool clear) {
        if(!Ready() || index>=edges.size())return false;
        auto& e=edges[index];if(e.state<0)--pending;
        int state=clear?1:0;if(e.state!=state)regionsReady=false;e.state=state;return true;
    }
    bool SampleAt(size_t index,ArenaPoint& point)const {if(index>=nodes.size())return false;point=nodes[index].point;return true;}
    int RegionAtSample(size_t index)const {if(!Verified() || index>=nodes.size())return -1;Regions();return regions[index];}
    int RegionAt(const ArenaPoint& point,bool(*line)(const ArenaPoint&,const ArenaPoint&,void*)=nullptr,void* context=nullptr)const {
        if(!Verified() || !Finite(point))return -1;int at=Nearest(point,line,context);return at<0?-1:RegionAtSample(static_cast<size_t>(at));
    }
    bool AnchorAt(const ArenaPoint& point,ArenaPoint& output,
        bool(*line)(const ArenaPoint&,const ArenaPoint&,void*)=nullptr,void* context=nullptr)const {
        if(!Verified() || !Finite(point))return false;int at=Nearest(point,line,context);if(at<0)return false;
        output=nodes[static_cast<size_t>(at)].point;return true;
    }
    bool ReachableSampleAt(size_t index,int region,ArenaPoint& point,
        bool(*line)(const ArenaPoint&,const ArenaPoint&,void*)=nullptr,void* context=nullptr)const {
        if(region<0 || RegionAtSample(index)!=region || !SampleAt(index,point))return false;
        return RegionAt(point,line,context)==region;
    }
    bool Path(const ArenaPoint& start,const ArenaPoint& goal,std::vector<ArenaPoint>& output,
        bool(*line)(const ArenaPoint&,const ArenaPoint&,void*)=nullptr,void* context=nullptr,
        bool(*traverse)(const ArenaPoint&,const ArenaPoint&,void*)=nullptr,void* traversalContext=nullptr)const {
        output.clear();if(!Verified() || !Finite(start) || !Finite(goal))return false;
        int first=Nearest(start,line,context,traverse,traversalContext);
        if(first<0)return false;
        int last=Nearest(goal,line,context,traverse,traversalContext,true);
        if(first<0 || last<0 || RegionAtSample(first)!=RegionAtSample(last))return false;
        struct Visit {int index;float cost;bool operator<(const Visit& b)const{return cost>b.cost;}};
        std::priority_queue<Visit> open;std::vector<float> cost(nodes.size(),1.e30f);std::vector<int> parent(nodes.size(),-1);
        std::vector<bool> closed(nodes.size(),false);cost[first]=0;open.push({first,0});
        while(!open.empty()) {
            int at=open.top().index;open.pop();if(closed[at])continue;closed[at]=true;if(at==last)break;
            for(int index:nodes[at].links) {
                const auto& e=edges[index];if(e.state!=1)continue;int to=e.a==at?e.b:e.a;if(closed[to])continue;
                float value=cost[at]+ArenaDistance(nodes[at].point,nodes[to].point)+std::fabs(nodes[at].point.z-nodes[to].point.z);
                if(value>=cost[to] || (traverse && !traverse(nodes[at].point,nodes[to].point,traversalContext)))continue;
                cost[to]=value;parent[to]=at;open.push({to,value+ArenaDistance(nodes[to].point,nodes[last].point)});
            }
        }
        if(!closed[last])return false;
        std::vector<int> reverse;for(int at=last;at>=0;at=parent[at]){reverse.push_back(at);if(at==first)break;}
        if(reverse.empty() || reverse.back()!=first)return false;
        std::vector<int> ordered(reverse.rbegin(),reverse.rend());size_t at=0;output.push_back(nodes[ordered[0]].point);
        while(at+1<ordered.size()) {
            // A traversal-qualified path retains its checked graph edges.
            // Visibility smoothing cannot bypass a body/dynamic-edge filter.
            size_t next=smoothRoutes && !traverse?(std::min)(at+12,ordered.size()-1):at+1;
            while(next>at+1 && !Visible(ordered[at],ordered[next],line,context))--next;
            output.push_back(nodes[ordered[next]].point);at=next;
        }
        if(line && !line(nodes[last].point,goal,context)){output.clear();return false;}
        output.push_back(goal);return true;
    }
};
}
