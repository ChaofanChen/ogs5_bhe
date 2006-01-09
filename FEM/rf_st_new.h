/**************************************************************************
FEMLib - Object: Source Terms ST
Task: class implementation
Programing:
01/2003 OK Implementation
last modified
**************************************************************************/
#ifndef rf_st_new_INC
#define rf_st_new_INC
// C++ STL
#include <list>
#include <fstream>
#include <string>
#include <vector>
using namespace std;
#include "rf_node.h"
#include "geo_ply.h"

//========================================================================
class CSourceTerm
{
  private:
    int CurveIndex; // Time funtion index
  public: //OK
    vector<int> PointsHaveDistribedBC;
    vector<double> DistribedBC;
    vector<double> DistBC_KRiverBed;
    vector<double> DistBC_WRiverBed;
    vector<double> DistBC_TRiverBed;
    vector<double> DistBC_BRiverBed;
    vector<double*>pnt_parameter_vector; //OK
  private: 
    CGLPolyline *plyST; //OK ???
    friend class CSourceTermGroup;
  public:
    // PCS
    //string primary_variable;
    string pcs_pv_name; //OK
    string pcs_type_name;
    int pcs_number;
    string pcs_type_name_cond; //OK
    string pcs_pv_name_cond; //OK
    // GEO
    string geo_prop_name;
    long geo_node_number;
    double geo_node_value; //Maxium three points
    string geo_type_name;
    long msh_node_number;
    int geo_id; //OK
    // DIS
    string dis_type_name;
    int component;
    int geo_type;
    int dis_type;
    double dis_prop[3];
    double *nodes;
    vector<long>node_number_vector;
    vector<double>node_value_vector;
    vector<long>node_renumber_vector;
    string delimiter_type;
    string geo_name;
    bool conditional;
    //double node_value_cond; //OK weg //MB
    //double condition; //OK
    // TIM
    int curve;
    string tim_type_name;
    // DISPLAY
    int display_mode;
    //Analytical term for matrix diffusion
    bool analytical;
    int analytical_material_group;
    double st_area;
    double analytical_diffusion;
    int number_of_terms;
    


    //--------------------------------------------------------------------
    // Methods
	CSourceTerm(void);
    ~CSourceTerm(void);
    ios::pos_type Read(ifstream*);
    void Write(fstream*);
    void SetDISType(void);
    void SetGEOType(void);
    void EdgeIntegration(CRFProcess* m_pcs, vector<long>&nodes_on_ply, 
                        vector<double>&node_value_vector);
    void FaceIntegration(CRFProcess* m_pcs, vector<long>&nodes_on_sfc, 
                        vector<double>&node_value_vector);
    void DomainIntegration(CRFProcess* m_pcs, vector<long>&nodes_in_dom, 
                        vector<double>&node_value_vector);
    // Set Members. WW
    void SetPolyline(CGLPolyline *Polyline) {plyST = Polyline;}  //OK ???

    void SetNOD2MSHNOD(vector<long>&nodes, vector<long>&conditional_nodes);
};

//========================================================================
class CSourceTermGroup
{
  public:
    CSourceTermGroup() {OrigSize=0;} //WW
    CSourceTermGroup* Get(string);
    void Set(CRFProcess* m_pcs, const int ShiftInNodeVector, string this_pv_name="");
    vector<CNodeValue*>group_vector;
    string pcs_name;
    string pcs_type_name; //OK
    string pcs_pv_name; //OK
    // Access to members
    void RecordNodeVSize(const int Size) {OrigSize = Size;}
    int GetOrigNodeVSize () const {return OrigSize;}
    CFEMesh* m_msh;
    vector<CSourceTerm*>st_group_vector; //OK
    double GetConditionalNODValue(int,CSourceTerm*); //OK
    double GetAnalyticalSolution(CSourceTerm *m_st,long node_number, string process);//CMCD
    void Write(ostream& os=cout) const; //WW
    void Read(istream& is=cin);  //WW
  private:
    int OrigSize;  // For excavation simulation. WW
    void SetPLY(CSourceTerm*); //OK
};

extern CSourceTermGroup* STGetGroup(string pcs_type_name,string pcs_pv_name);
extern list<CSourceTermGroup*>st_group_list;
extern bool STRead(string);
extern void STWrite(string);
#define ST_FILE_EXTENSION ".st"
extern void STCreateFromPNT();
extern vector<CSourceTerm*>st_vector;
extern void STDelete();
void STCreateFromLIN(vector<CGLLine*>);
CSourceTerm* STGet(string);
extern void STGroupDelete(string pcs_type_name,string pcs_pv_name);
extern long aktueller_zeitschritt;
extern double aktuelle_zeit;
extern vector<string>analytical_processes;

#endif
