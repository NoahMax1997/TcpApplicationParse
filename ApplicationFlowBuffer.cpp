#include "ApplicationFlowBuffer.h"
const char CR='\r';
const char LF='\n';
const int FLOW_BUFF_MAX=65536;
ApplicationFlowBuffer::ApplicationFlowBuffer(/* args */)
{
    buffer_length_=0;
    buffer_=nullptr;
    orig_data_begin_=nullptr;
    orig_data_end_=nullptr;
    ResetLineState();
    mode_=UNKNOWN_MODE;
    frame_length_=0;
    chunked_=false;
    data_seq_at_orig_data_end=0;
    eof_=false;
    have_pending_request_=false;
    buffer_n_=0;
    NewMessage();
}
ApplicationFlowBuffer::ApplicationFlowBuffer(LineBreakStyle line_style)
{
    ApplicationFlowBuffer();
    linebreak_style_=line_style;
}

ApplicationFlowBuffer::~ApplicationFlowBuffer()
{
    if(buffer_){
        free(buffer_);
    }
}
void ApplicationFlowBuffer::NewMessage(){
    assert(frame_length_>=0);
    int byte_to_advance=0;
    if(buffer_n_==0){
        switch (mode_)
        {
        case LINE_MODE:
            /* code */
            byte_to_advance=(frame_length_+(linebreak_style_==STRICT_CRLF?2:1));
            break;
        case FRAME_MODE:
            /* code */
            byte_to_advance=frame_length_;
            break;
        case UNKNOWN_MODE:
            /* code */
            break;
        default:
            break;
        }
        orig_data_begin_+=byte_to_advance;
        assert(orig_data_begin_<=orig_data_end_);
        buffer_n_=0;
        message_complete_=false;
    }
}
void ApplicationFlowBuffer::ResetLineState(){
    switch (linebreak_style_)
    {
    case CR_OR_LF:
        /* code */
        state_=CR_OR_LF_0;
        break;
    case STRICT_CRLF:
        /* code */
        state_=STRICT_CRLF_0;
        break;
    default:
        assert(0);
        break;
    }
}
bool ApplicationFlowBuffer::ExpandBuffer(int length){
    bool ret=true;
    if(length>=FLOW_BUFF_MAX){
        ret=false;
    }else{
        if(length<512){
            length=512;
        }
        if(length<buffer_length_*2){
            length=buffer_length_*2;
        }
        unsigned char *new_buf =(unsigned char*)realloc(buffer_,length);
        if(new_buf==nullptr){
            ret=false;
        }else{
            //memory realloc successfully
            buffer_length_=length;
            buffer_=new_buf;
        }
    }
    return ret;
}
void ApplicationFlowBuffer::NewLine(){
    NewMessage();
    mode_=LINE_MODE;
    frame_length_=0;
    chunked_=false;
    have_pending_request_=true;
    MarkOrCopyFrame();
}
void ApplicationFlowBuffer::NewFrame(int frame_length,bool chunked){
    NewMessage();
    mode_=FRAME_MODE;
    frame_length_=frame_length;
    chunked_=chunked;
    have_pending_request_=true;
    MarkOrCopyFrame();
}
void ApplicationFlowBuffer::BufferData(BytePoint data,BytePoint end){
    if(data<end){
        mode_=FRAME_MODE;
        frame_length_+=end-data;
        MarkOrCopyFrame();
        NewData(data,end);
    }
}
void ApplicationFlowBuffer::FinishBuffer(){
    message_complete_=true;
}
void ApplicationFlowBuffer::GrowFrame(int length){
    assert(frame_length_>=0);
    if(length<=frame_length_){
        return;
    }
    assert(!chunked_||frame_length_==0);
    mode_=FRAME_MODE;
    frame_length_=length;
    MarkOrCopyFrame();
}
void ApplicationFlowBuffer::DiscardData(){
    mode_=UNKNOWN_MODE;
    message_complete_=false;
    have_pending_request_=false;
    orig_data_begin_=orig_data_end_=0;
    buffer_n_=0;
    frame_length_=0;
}
void ApplicationFlowBuffer::set_eof(){
    eof_=true;
    if(chunked_){
        frame_length_=orig_data_end_-orig_data_begin_;
    }
    if(frame_length_<0){
        frame_length_=0;
    }
}
void ApplicationFlowBuffer::NewData(BytePoint begin,BytePoint end){
    assert(begin<end);
    ClearPreviousData();
    assert((buffer_n_==0&&message_complete_)||orig_data_begin_==orig_data_end_);
    orig_data_begin_=begin;
    orig_data_end_=end;
    data_seq_at_orig_data_end+=(end-begin);
    MarkOrCopy();
}
void ApplicationFlowBuffer::NewData(const string &data){
    BytePoint begin=(BytePoint)(data.c_str());
    BytePoint end=begin+data.size();
    NewData(begin,end);
}
void ApplicationFlowBuffer::MarkOrCopy(){
    if(!message_complete_){
        switch (mode_)
        {
        case LINE_MODE:
            /* code */
            MarkOrCopyLine();
            break;
        case FRAME_MODE:
            /* code */
            MarkOrCopyFrame();
            break;
        default:
            break;
        }
    }
}
void ApplicationFlowBuffer::ClearPreviousData(){
    //previous data must have been processed or buffered already
    
}