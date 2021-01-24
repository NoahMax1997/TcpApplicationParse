#ifndef _APPLICATION_FLOW_BUFFER_H_
#define _APPLICATION_FLOW_BUFFER_H_
#include "CommonHead.h"
#include <assert.h>
class ApplicationFlowBuffer
{
private:
    /* data */
    enum LineBreakStyle
    {
        CR_OR_LF,
        STRICT_CRLF,
        CR_LF_NUL,
    };
    enum
    {
        UNKNOWN_MODE,
        LINE_MODE,
        FRAME_MODE,
    } mode_;
    enum
    {
        CR_OR_LF_0,
        CR_OR_LF_1,
        STRICT_CRLF_0,
        STRICT_CRLF_1,
        FRAME_0,
    } state_;
    int buffer_n_;       //num of bytes in buffer
    int buffer_length_; //size of the buffer
    unsigned char *buffer_;
    bool message_complete_; //message whether complete
    int frame_length_;
    bool chunked_;
    BytePoint orig_data_begin_, orig_data_end_;
    LineBreakStyle linebreak_style_;
    int data_seq_at_orig_data_end;
    bool eof_;
    bool have_pending_request_;
    void NewMessage();
    void ClearPreviousData();
    bool ExpandBuffer(int length);
    void ResetLineState();
    void AppendToBuffer(BytePoint data,int len);
    void MarkOrCopy();
    void MarkOrCopyLine();
    void MarkOrCopyFrame();
    void MarkOrCopyLine_CR_OR_LF();
    void MarkOrCopyLine_STRICT_CRLF();
public : 
    ApplicationFlowBuffer(/* args */);
    ApplicationFlowBuffer(LineBreakStyle line_style);
    virtual ~ApplicationFlowBuffer();
    void NewData(BytePoint begin, BytePoint end);
    void NewData(const string &data);
    void NewGap(int length);
    void BufferData(BytePoint data, BytePoint end);
    void FinishBuffer();
    void DiscardData();
    inline bool ready() const
    {
        return message_complete_ || mode_ == UNKNOWN_MODE;
    }
    inline BytePoint begin() const
    {
        assert(ready());
        return (buffer_n_ == 0) ? orig_data_begin_ : buffer_;
    }
    inline BytePoint end() const
    {
        assert(ready());
        BytePoint ret;
        if (buffer_n_ == 0)
        {
            assert(frame_length_ >= 0);
            ret = orig_data_begin_ + frame_length_;
            assert(ret <= orig_data_end_);
        }
        else
        {
            ret = buffer_ + buffer_n_;
        }
        return ret;
    }
    inline int data_length() const{
        int ret=frame_length_;
        if(buffer_n_>0){
            ret=buffer_n_;
        }
        if(frame_length_<0||orig_data_begin_+frame_length_>orig_data_end_){
            ret= orig_data_end_-orig_data_begin_;
        }
        return ret;
    }
    inline bool data_avaliable() const{
        return buffer_n_>0||orig_data_end_>orig_data_begin_;
    }
    void NewLine();
    void NewFrame(int frame_length,bool chunked);
    void GrowFrame(int new_frame_length);
    int data_seq() const{
        int data_seq_at_orig_data_begin=data_seq_at_orig_data_end-(orig_data_end_-orig_data_begin_);
        if(buffer_n_>0){
            return data_seq_at_orig_data_begin;
        }else{
            return data_seq_at_orig_data_begin+data_length();
        }
    }
    bool eof() const{
        return eof_;
    }
    void set_eof();
    bool have_pending_request() const{
        return have_pending_request_;
    }
};

#endif